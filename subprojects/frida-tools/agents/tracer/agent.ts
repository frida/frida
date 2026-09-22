import type _Java from "frida-java-bridge";

const MAX_HANDLERS_PER_REQUEST = 1000;

class Agent {
    private handlers = new Map<TraceTargetId, TraceHandler>();
    private nativeTargets = new Set<string>();
    private stagedPlanRequest: TracePlanRequest | null = null;
    private stackDepth = new Map<ThreadId, number>();
    private traceState: TraceState = {};
    private nextId = 1;
    private started = Date.now();

    private pendingEvents: TraceEvent[] = [];
    private flushTimer: any = null;

    private cachedModuleResolver: ApiResolver | null = null;
    private cachedObjcResolver: ApiResolver | null = null;
    private cachedSwiftResolver: ApiResolver | null = null;

    init(stage: Stage, parameters: TraceParameters, initScripts: InitScript[], spec: TraceSpec) {
        globalThis.stage = stage;
        globalThis.parameters = parameters;
        globalThis.state = this.traceState;
        globalThis.defineHandler = h => h;

        registerLazyBridgeGetter("ObjC");
        registerLazyBridgeGetter("Swift");
        registerLazyBridgeGetter("Java");

        for (const script of initScripts) {
            Script.evaluate(script.filename, script.source);
        }

        this.start(spec).catch(e => {
            send({
                type: "agent:error",
                message: e.message
            });
        });

        return {
            id: Process.id,
            platform: Process.platform,
            arch: Process.arch,
            pointer_size: Process.pointerSize,
            page_size: Process.pageSize,
            main_module: Process.mainModule,
        };
    }

    dispose() {
        this.flush();
    }

    updateHandlerCode(id: TraceTargetId, name: string, script: HandlerScript) {
        const handler = this.handlers.get(id);
        if (handler === undefined) {
            throw new Error("invalid target ID");
        }

        if (handler.length === 3) {
            const newHandler = this.parseFunctionHandler(script, id, name, this.onTraceError);
            handler[0] = newHandler[0];
            handler[1] = newHandler[1];
        } else {
            const newHandler = this.parseInstructionHandler(script, id, name, this.onTraceError);
            handler[0] = newHandler[0];
        }
    }

    updateHandlerConfig(id: TraceTargetId, config: HandlerConfig) {
        const handler = this.handlers.get(id);
        if (handler === undefined) {
            throw new Error("invalid target ID");
        }

        handler[2] = config;
    }

    async stageTargets(spec: TraceSpec): Promise<StagedItem[]> {
        const request = await this.createPlan(spec);
        this.stagedPlanRequest = request;
        await request.ready;
        const { plan } = request;

        const items: StagedItem[] = [];
        let id: StagedItemId = 1;
        for (const [type, scope, member] of plan.native.values()) {
            items.push([ id, scope, member ]);
            id++;
        }
        id = -1;
        for (const group of plan.java) {
            for (const [className, classDetails] of group.classes.entries()) {
                for (const methodName of classDetails.methods.values()) {
                    items.push([ id, className, methodName ]);
                    id--;
                }
            }
        }
        return items;
    }

    async commitTargets(id: StagedItemId | null): Promise<CommitResult> {
        const request = this.stagedPlanRequest!;
        this.stagedPlanRequest = null;

        let { plan } = request;
        if (id !== null) {
            plan = this.cropStagedPlan(plan, id);
        }

        const errorEvents: TraceError[] = [];
        const onError: TraceErrorEventHandler = e => {
            errorEvents.push(e);
        };

        const nativeIds = await this.traceNativeTargets(plan.native, onError);

        let javaIds: TraceTargetId[] = [];
        if (plan.java.length !== 0) {
            javaIds = await new Promise<TraceTargetId[]>((resolve, reject) => {
                globalThis.Java.perform(() => {
                    this.traceJavaTargets(plan.java, onError).then(resolve, reject);
                });
            });
        }

        return {
            ids: [...nativeIds, ...javaIds],
            errors: errorEvents,
        };
    }

    readMemory(address: string, size: number): ArrayBuffer | null {
        try {
            return ptr(address).readVolatile(size);
        } catch (e) {
            return null;
        }
    }

    resolveAddresses(addresses: string[]): string[] {
        let cachedModules: ModuleMap | null = null;
        return addresses
            .map(ptr)
            .map(DebugSymbol.fromAddress)
            .map(sym => {
                if (sym.name === null) {
                    if (cachedModules === null) {
                        cachedModules = new ModuleMap();
                    }
                    const module = cachedModules.find(sym.address);
                    if (module !== null) {
                        return `${module.name}!${sym.address.sub(module.base)}`;
                    }
                }
                return sym;
            })
            .map(s => s.toString());
    }

    private cropStagedPlan(plan: TracePlan, id: StagedItemId): TracePlan {
        let candidateId: StagedItemId;

        if (id < 0) {
            candidateId = -1;
            for (const group of plan.java) {
                for (const [className, classDetails] of group.classes.entries()) {
                    for (const [methodName, methodNameOrSignature] of classDetails.methods.entries()) {
                        if (candidateId === id) {
                            const croppedMethods = new Map([[methodName, methodNameOrSignature]]);
                            const croppedClass: JavaTargetClass = { methods: croppedMethods };
                            const croppedGroup: JavaTargetGroup = { loader: group.loader, classes: new Map([[className, croppedClass]]) };
                            const croppedPlan = new TracePlan();
                            croppedPlan.java.push(croppedGroup);
                            return croppedPlan;
                        }
                        candidateId--;
                    }
                }
            }
        } else {
            candidateId = 1;
            for (const [k, v] of plan.native.entries()) {
                if (candidateId === id) {
                    const croppedPlan = new TracePlan();
                    croppedPlan.native.set(k, v);
                    return croppedPlan;
                }
                candidateId++;
            }
        }

        throw new Error("invalid staged item ID");
    }

    private async start(spec: TraceSpec) {
        const onJavaReady = async (plan: TracePlan) => {
            await this.traceJavaTargets(plan.java, this.onTraceError);
        };

        const request = await this.createPlan(spec, onJavaReady);

        await this.traceNativeTargets(request.plan.native, this.onTraceError);

        send({
            type: "agent:initialized"
        });

        request.ready.then(() => {
            send({
                type: "agent:started",
                count: this.handlers.size
            });
        });
    }

    private onTraceError: TraceErrorEventHandler = ({ id, name, message }) => {
        send({
            type: "agent:warning",
            id,
            message: `Skipping "${name}": ${message}`
        });
    };

    private async createPlan(spec: TraceSpec,
            onJavaReady: (plan: TracePlan) => Promise<void> = async () => {}): Promise<TracePlanRequest> {
        const plan = new TracePlan();

        const javaEntries: [TraceSpecOperation, TraceSpecPattern][] = [];
        for (const [operation, scope, pattern] of spec) {
            switch (scope) {
                case "module":
                    if (operation === "include") {
                        this.includeModule(pattern, plan);
                    } else {
                        this.excludeModule(pattern, plan);
                    }
                    break;
                case "function":
                    if (operation === "include") {
                        this.includeFunction(pattern, plan);
                    } else {
                        this.excludeFunction(pattern, plan);
                    }
                    break;
                case "relative-function":
                    if (operation === "include") {
                        this.includeRelativeFunction(pattern, plan);
                    }
                    break;
                case "absolute-instruction":
                    if (operation === "include") {
                        this.includeAbsoluteInstruction(ptr(pattern), plan);
                    }
                    break;
                case "imports":
                    if (operation === "include") {
                        this.includeImports(pattern, plan);
                    }
                    break;
                case "objc-method":
                    if (operation === "include") {
                        this.includeObjCMethod(pattern, plan);
                    } else {
                        this.excludeObjCMethod(pattern, plan);
                    }
                    break;
                case "swift-func":
                    if (operation === "include") {
                        this.includeSwiftFunc(pattern, plan);
                    } else {
                        this.excludeSwiftFunc(pattern, plan);
                    }
                    break;
                case "java-method":
                    javaEntries.push([operation, pattern]);
                    break;
                case "debug-symbol":
                    if (operation === "include") {
                        this.includeDebugSymbol(pattern, plan);
                    }
                    break;
            }
        }

        for (const address of plan.native.keys()) {
            if (this.nativeTargets.has(address)) {
                plan.native.delete(address);
            }
        }

        let javaStartRequest: Promise<void>;
        let javaStartDeferred = true;
        if (javaEntries.length > 0) {
            const Java = globalThis.Java;

            if (!Java.available) {
                throw new Error("Java runtime is not available");
            }

            javaStartRequest = new Promise((resolve, reject) => {
                Java.perform(async () => {
                    javaStartDeferred = false;

                    try {
                        for (const [operation, pattern] of javaEntries) {
                            if (operation === "include") {
                                this.includeJavaMethod(pattern, plan);
                            } else {
                                this.excludeJavaMethod(pattern, plan);
                            }
                        }

                        await onJavaReady(plan);

                        resolve();
                    } catch (e) {
                        reject(e);
                    }
                });
            });
        } else {
            javaStartRequest = Promise.resolve();
        }

        if (!javaStartDeferred) {
            await javaStartRequest;
        }

        return { plan, ready: javaStartRequest };
    }

    private async traceNativeTargets(targets: NativeTargets, onError: TraceErrorEventHandler): Promise<TraceTargetId[]> {
        const insnGroups = new Map<string, NativeItem[]>();
        const cGroups = new Map<string, NativeItem[]>();
        const objcGroups = new Map<string, NativeItem[]>();
        const swiftGroups = new Map<string, NativeItem[]>();

        for (const [id, [type, scope, name]] of targets.entries()) {
            let entries: Map<string, NativeItem[]>;
            switch (type) {
                case "insn":
                    entries = insnGroups;
                    break;
                case "c":
                    entries = cGroups;
                    break;
                case "objc":
                    entries = objcGroups;
                    break;
                case "swift":
                    entries = swiftGroups;
                    break;
            }

            let group = entries.get(scope);
            if (group === undefined) {
                group = [];
                entries.set(scope, group);
            }

            group.push([name, ptr(id)]);
        }

        const [cIds, objcIds, swiftIds] = await Promise.all([
            this.traceNativeEntries("insn", insnGroups, onError),
            this.traceNativeEntries("c", cGroups, onError),
            this.traceNativeEntries("objc", objcGroups, onError),
            this.traceNativeEntries("swift", swiftGroups, onError),
        ]);

        return [...cIds, ...objcIds, ...swiftIds];
    }

    private async traceNativeEntries(flavor: NativeTargetFlavor, groups: NativeTargetScopes, onError: TraceErrorEventHandler):
            Promise<TraceTargetId[]> {
        if (groups.size === 0) {
            return [];
        }

        const baseId = this.nextId;
        const scopes: HandlerRequestScope[] = [];
        const request: HandlerRequest = {
            type: "handlers:get",
            flavor,
            baseId,
            scopes
        };
        for (const [name, items] of groups.entries()) {
            scopes.push({
                name,
                members: items.map(item => item[0]),
                addresses: items.map(item => item[1].toString())
            });
            this.nextId += items.length;
        }

        const { scripts }: HandlerResponse = await getHandlers(request);

        const ids: TraceTargetId[] = [];
        let offset = 0;
        const isInstruction = flavor === "insn";
        for (const items of groups.values()) {
            for (const [name, address] of items) {
                const id = baseId + offset;
                const displayName = (typeof name === "string") ? name : name[1];

                const handler = isInstruction
                    ? this.parseInstructionHandler(scripts[offset], id, displayName, onError)
                    : this.parseFunctionHandler(scripts[offset], id, displayName, onError);
                this.handlers.set(id, handler);
                this.nativeTargets.add(address.toString());

                try {
                    Interceptor.attach(address, isInstruction
                            ? this.makeNativeInstructionListener(id, handler as TraceInstructionHandler)
                            : this.makeNativeFunctionListener(id, handler as TraceFunctionHandler));
                } catch (e) {
                    onError({ id, name: displayName, message: (e as Error).message });
                }

                ids.push(id);
                offset++;
            }
        }
        return ids;
    }

    private async traceJavaTargets(groups: JavaTargetGroup[], onError: TraceErrorEventHandler): Promise<TraceTargetId[]> {
        const baseId = this.nextId;
        const scopes: HandlerRequestScope[] = [];
        const request: HandlerRequest = {
            type: "handlers:get",
            flavor: "java",
            baseId,
            scopes
        };
        for (const group of groups) {
            for (const [className, { methods }] of group.classes.entries()) {
                const classNameParts = className.split(".");
                const bareClassName = classNameParts[classNameParts.length - 1];
                const members: MemberName[] = Array.from(methods.keys()).map(bareName => [bareName, `${bareClassName}.${bareName}`]);
                scopes.push({
                    name: className,
                    members
                });
                this.nextId += members.length;
            }
        }

        const { scripts }: HandlerResponse = await getHandlers(request);

        return new Promise<TraceTargetId[]>(resolve => {
            const Java = globalThis.Java;

            Java.perform(() => {
                const ids: TraceTargetId[] = [];
                let offset = 0;
                for (const group of groups) {
                    const factory = Java.ClassFactory.get(group.loader as any);

                    for (const [className, { methods }] of group.classes.entries()) {
                        const C = factory.use(className);

                        for (const [bareName, fullName] of methods.entries()) {
                            const id = baseId + offset;

                            const handler = this.parseFunctionHandler(scripts[offset], id, fullName, onError);
                            this.handlers.set(id, handler);

                            const dispatcher = C[bareName];
                            for (const method of dispatcher.overloads) {
                                method.implementation = this.makeJavaMethodWrapper(id, method, handler);
                            }

                            ids.push(id);
                            offset++;
                        }
                    }
                }

                resolve(ids);
            });
        });
    }

    private makeNativeFunctionListener(id: TraceTargetId, handler: TraceFunctionHandler): InvocationListenerCallbacks {
        const agent = this;

        return {
            onEnter(args) {
                const [onEnter, _, config] = handler;
                agent.invokeNativeHandler(id, onEnter, config, this, args, ">");
            },
            onLeave(retval) {
                const [_, onLeave, config] = handler;
                agent.invokeNativeHandler(id, onLeave, config, this, retval, "<");
            }
        };
    }

    private makeNativeInstructionListener(id: TraceTargetId, handler: TraceInstructionHandler): InstructionProbeCallback {
        const agent = this;

        return function (args) {
            const [onHit, config] = handler;
            agent.invokeNativeHandler(id, onHit, config, this, args, "|");
        };
    }

    private makeJavaMethodWrapper(id: TraceTargetId, method: _Java.Method, handler: TraceFunctionHandler): _Java.MethodImplementation {
        const agent = this;

        return function (...args: any[]) {
            return agent.handleJavaInvocation(id, method, handler, this, args);
        };
    }

    private handleJavaInvocation(id: TraceTargetId, method: _Java.Method, handler: TraceFunctionHandler, instance: _Java.Wrapper, args: any[]): any {
        const [onEnter, onLeave, config] = handler;

        this.invokeJavaHandler(id, onEnter, config, instance, args, ">");

        const retval = method.apply(instance, args);

        const replacementRetval = this.invokeJavaHandler(id, onLeave, config, instance, retval, "<");

        return (replacementRetval !== undefined) ? replacementRetval : retval;
    }

    private invokeNativeHandler(id: TraceTargetId, callback: TraceEnterHandler | TraceLeaveHandler | TraceProbeHandler,
            config: HandlerConfig, context: InvocationContext, param: any, cutPoint: CutPoint) {
        const threadId = context.threadId;
        const depth = this.updateDepth(threadId, cutPoint);

        if (config.muted) {
            return;
        }

        const timestamp = Date.now() - this.started;
        const caller = context.returnAddress.toString();
        const backtrace = config.capture_backtraces ? Thread.backtrace(context.context).map(p => p.toString()) : null;

        const log = (...message: string[]) => {
            this.emit([id, timestamp, threadId, depth, caller, backtrace, message.join(" ")]);
        };

        callback.call(context, log, param, this.traceState);
    }

    private invokeJavaHandler(id: TraceTargetId, callback: TraceEnterHandler | TraceLeaveHandler, config: HandlerConfig,
            instance: _Java.Wrapper, param: any, cutPoint: CutPoint) {
        const threadId = Process.getCurrentThreadId();
        const depth = this.updateDepth(threadId, cutPoint);

        if (config.muted) {
            return;
        }

        const timestamp = Date.now() - this.started;

        const log = (...message: string[]) => {
            this.emit([id, timestamp, threadId, depth, null, null, message.join(" ")]);
        };

        try {
            return callback.call(instance, log, param, this.traceState);
        } catch (e: any) {
            const isJavaException = e.$h !== undefined;
            if (isJavaException) {
                throw e;
            } else {
                Script.nextTick(() => { throw e; });
            }
        }
    }

    private updateDepth(threadId: ThreadId, cutPoint: CutPoint): number {
        const depthEntries = this.stackDepth;

        let depth = depthEntries.get(threadId) ?? 0;
        if (cutPoint === ">") {
            depthEntries.set(threadId, depth + 1);
        } else if (cutPoint === "<") {
            depth--;
            if (depth !== 0) {
                depthEntries.set(threadId, depth);
            } else {
                depthEntries.delete(threadId);
            }
        }

        return depth;
    }

    private parseFunctionHandler(script: string, id: TraceTargetId, name: string, onError: TraceErrorEventHandler): TraceFunctionHandler {
        try {
            const h = this.parseHandlerScript(name, script);
            return [h.onEnter ?? noop, h.onLeave ?? noop, makeDefaultHandlerConfig()];
        } catch (e) {
            onError({ id, name, message: (e as Error).message });
            return [noop, noop, makeDefaultHandlerConfig()];
        }
    }

    private parseInstructionHandler(script: string, id: TraceTargetId, name: string, onError: TraceErrorEventHandler):
            TraceInstructionHandler {
        try {
            const onHit = this.parseHandlerScript(name, script);
            return [onHit, makeDefaultHandlerConfig()];
        } catch (e) {
            onError({ id, name, message: (e as Error).message });
            return [noop, makeDefaultHandlerConfig()];
        }
    }

    private parseHandlerScript(name: string, script: string): any {
        const id = `/handlers/${name}.js`;
        return Script.evaluate(id, script);
    }

    private includeModule(pattern: string, plan: TracePlan) {
        const { native } = plan;
        for (const m of this.getModuleResolver().enumerateMatches(`exports:${pattern}!*`)) {
            native.set(m.address.toString(), moduleFunctionTargetFromMatch(m));
        }
    }

    private excludeModule(pattern: string, plan: TracePlan) {
        const { native } = plan;
        for (const m of this.getModuleResolver().enumerateMatches(`exports:${pattern}!*`)) {
            native.delete(m.address.toString());
        }
    }

    private includeFunction(pattern: string, plan: TracePlan) {
        const e = parseModuleFunctionPattern(pattern);
        const { native } = plan;
        for (const m of this.getModuleResolver().enumerateMatches(`exports:${e.module}!${e.function}`)) {
            native.set(m.address.toString(), moduleFunctionTargetFromMatch(m));
        }
    }

    private excludeFunction(pattern: string, plan: TracePlan) {
        const e = parseModuleFunctionPattern(pattern);
        const { native } = plan;
        for (const m of this.getModuleResolver().enumerateMatches(`exports:${e.module}!${e.function}`)) {
            native.delete(m.address.toString());
        }
    }

    private includeRelativeFunction(pattern: string, plan: TracePlan) {
        const e = parseRelativeFunctionPattern(pattern);
        const address = Process.getModuleByName(e.module).base.add(e.offset);
        plan.native.set(address.toString(), ["c", e.module, `sub_${e.offset.toString(16)}`]);
    }

    private includeAbsoluteInstruction(address: NativePointer, plan: TracePlan) {
        const module = plan.modules.find(address);
        if (module !== null) {
            plan.native.set(address.toString(), ["insn", module.path, `insn_${address.sub(module.base).toString(16)}`]);
        } else {
            plan.native.set(address.toString(), ["insn", "", `insn_${address.toString(16)}`]);
        }
    }

    private includeImports(pattern: string, plan: TracePlan) {
        let matches: ApiResolverMatch[];
        if (pattern === null) {
            const mainModule = Process.enumerateModules()[0].path;
            matches = this.getModuleResolver().enumerateMatches(`imports:${mainModule}!*`);
        } else {
            matches = this.getModuleResolver().enumerateMatches(`imports:${pattern}!*`);
        }

        const { native } = plan;
        for (const m of matches) {
            native.set(m.address.toString(), moduleFunctionTargetFromMatch(m));
        }
    }

    private includeObjCMethod(pattern: string, plan: TracePlan) {
        const { native } = plan;
        for (const m of this.getObjcResolver().enumerateMatches(pattern)) {
            native.set(m.address.toString(), objcMethodTargetFromMatch(m));
        }
    }

    private excludeObjCMethod(pattern: string, plan: TracePlan) {
        const { native } = plan;
        for (const m of this.getObjcResolver().enumerateMatches(pattern)) {
            native.delete(m.address.toString());
        }
    }

    private includeSwiftFunc(pattern: string, plan: TracePlan) {
        const { native } = plan;
        for (const m of this.getSwiftResolver().enumerateMatches(`functions:${pattern}`)) {
            native.set(m.address.toString(), swiftFuncTargetFromMatch(m));
        }
    }

    private excludeSwiftFunc(pattern: string, plan: TracePlan) {
        const { native } = plan;
        for (const m of this.getSwiftResolver().enumerateMatches(`functions:${pattern}`)) {
            native.delete(m.address.toString());
        }
    }

    private includeJavaMethod(pattern: string, plan: TracePlan) {
        const existingGroups = plan.java;

        const groups = Java.enumerateMethods(pattern);
        for (const group of groups) {
            const { loader } = group;

            const existingGroup = find(existingGroups, candidate => {
                const { loader: candidateLoader } = candidate;
                if (candidateLoader !== null && loader !== null) {
                    return candidateLoader.equals(loader);
                } else {
                    return candidateLoader === loader;
                }
            });
            if (existingGroup === undefined) {
                existingGroups.push(javaTargetGroupFromMatchGroup(group));
                continue;
            }

            const { classes: existingClasses } = existingGroup;
            for (const klass of group.classes) {
                const { name: className } = klass;

                const existingClass = existingClasses.get(className);
                if (existingClass === undefined) {
                    existingClasses.set(className, javaTargetClassFromMatchClass(klass));
                    continue;
                }

                const { methods: existingMethods } = existingClass;
                for (const methodName of klass.methods) {
                    const bareMethodName = javaBareMethodNameFromMethodName(methodName);
                    const existingName = existingMethods.get(bareMethodName);
                    if (existingName === undefined) {
                        existingMethods.set(bareMethodName, methodName);
                    } else {
                        existingMethods.set(bareMethodName, (methodName.length > existingName.length) ? methodName : existingName);
                    }
                }
            }
        }
    }

    private excludeJavaMethod(pattern: string, plan: TracePlan) {
        const existingGroups = plan.java;

        const groups = globalThis.Java.enumerateMethods(pattern);
        for (const group of groups) {
            const { loader } = group;

            const existingGroup = find(existingGroups, candidate => {
                const { loader: candidateLoader } = candidate;
                if (candidateLoader !== null && loader !== null) {
                    return candidateLoader.equals(loader);
                } else {
                    return candidateLoader === loader;
                }
            });
            if (existingGroup === undefined) {
                continue;
            }

            const { classes: existingClasses } = existingGroup;
            for (const klass of group.classes) {
                const { name: className } = klass;

                const existingClass = existingClasses.get(className);
                if (existingClass === undefined) {
                    continue;
                }

                const { methods: existingMethods } = existingClass;
                for (const methodName of klass.methods) {
                    const bareMethodName = javaBareMethodNameFromMethodName(methodName);
                    existingMethods.delete(bareMethodName);
                }
            }
        }
    }

    private includeDebugSymbol(pattern: string, plan: TracePlan) {
        const { native } = plan;
        for (const address of DebugSymbol.findFunctionsMatching(pattern)) {
            native.set(address.toString(), debugSymbolTargetFromAddress(address));
        }
    }

    private emit(event: TraceEvent) {
        this.pendingEvents.push(event);

        if (this.flushTimer === null) {
            this.flushTimer = setTimeout(this.flush, 50);
        }
    }

    private flush = () => {
        if (this.flushTimer !== null) {
            clearTimeout(this.flushTimer);
            this.flushTimer = null;
        }

        if (this.pendingEvents.length === 0) {
            return;
        }

        const events = this.pendingEvents;
        this.pendingEvents = [];

        send({
            type: "events:add",
            events
        });
    };

    private getModuleResolver(): ApiResolver {
        let resolver = this.cachedModuleResolver;
        if (resolver === null) {
            resolver = new ApiResolver("module");
            this.cachedModuleResolver = resolver;
        }
        return resolver;
    }

    private getObjcResolver(): ApiResolver {
        let resolver = this.cachedObjcResolver;
        if (resolver === null) {
            try {
                resolver = new ApiResolver("objc");
            } catch (e: any) {
                throw new Error("Objective-C runtime is not available");
            }
            this.cachedObjcResolver = resolver;
        }
        return resolver;
    }

    private getSwiftResolver(): ApiResolver {
        let resolver = this.cachedSwiftResolver;
        if (resolver === null) {
            try {
                resolver = new ApiResolver("swift" as ApiResolverType); // FIXME: Update typings.
            } catch (e: any) {
                throw new Error("Swift runtime is not available");
            }
            this.cachedSwiftResolver = resolver;
        }
        return resolver;
    }
}

async function getHandlers(request: HandlerRequest): Promise<HandlerResponse> {
    const scripts: HandlerScript[] = [];

    const { type, flavor, baseId } = request;

    const pendingScopes = request.scopes.slice().map(({ name, members, addresses }) => {
        return {
            name,
            members: members.slice(),
            addresses: addresses?.slice(),
        };
    });
    let id = baseId;
    do {
        const curScopes: HandlerRequestScope[] = [];
        const curRequest: HandlerRequest = {
            type,
            flavor,
            baseId: id,
            scopes: curScopes
        };

        let size = 0;
        for (const { name, members: pendingMembers, addresses: pendingAddresses } of pendingScopes) {
            const n = Math.min(pendingMembers.length, MAX_HANDLERS_PER_REQUEST - size);
            if (n === 0) {
                break;
            }
            curScopes.push({
                name,
                members: pendingMembers.splice(0, n),
                addresses: pendingAddresses?.splice(0, n),
            });
            size += n;
        }

        while (pendingScopes.length !== 0 && pendingScopes[0].members.length === 0) {
            pendingScopes.splice(0, 1);
        }

        send(curRequest);
        const response: HandlerResponse = await receiveResponse(`reply:${id}`);

        scripts.push(...response.scripts);

        id += size;
    } while (pendingScopes.length !== 0);

    return {
        scripts
    };
}

function makeDefaultHandlerConfig(): HandlerConfig {
    return {
        muted: false,
        capture_backtraces: false,
    };
}

function receiveResponse<T>(type: string): Promise<T> {
    return new Promise(resolve => {
        recv(type, (response: T) => {
            resolve(response);
        });
    });
}

function moduleFunctionTargetFromMatch(m: ApiResolverMatch): NativeTarget {
    const [modulePath, functionName] = m.name.split("!").slice(-2);
    return ["c", modulePath, functionName];
}

function objcMethodTargetFromMatch(m: ApiResolverMatch): NativeTarget {
    const { name } = m;
    const [className, methodName] = name.substr(2, name.length - 3).split(" ", 2);
    return ["objc", className, [methodName, name]];
}

function swiftFuncTargetFromMatch(m: ApiResolverMatch): NativeTarget {
    const { name } = m;
    const [modulePath, methodName] = name.split("!", 2);
    return ["swift", modulePath, methodName];
}

function debugSymbolTargetFromAddress(address: NativePointer): NativeTarget {
    const symbol = DebugSymbol.fromAddress(address);
    return ["c", symbol.moduleName ?? "", symbol.name!];
}

function parseModuleFunctionPattern(pattern: string) {
    const tokens = pattern.split("!", 2);

    let m, f;
    if (tokens.length === 1) {
        m = "*";
        f = tokens[0];
    } else {
        m = (tokens[0] === "") ? "*" : tokens[0];
        f = (tokens[1] === "") ? "*" : tokens[1];
    }

    return {
        module: m,
        function: f
    };
}

function parseRelativeFunctionPattern(pattern: string) {
    const tokens = pattern.split("!", 2);

    return {
        module: tokens[0],
        offset: parseInt(tokens[1], 16)
    };
}

function javaTargetGroupFromMatchGroup(group: _Java.EnumerateMethodsMatchGroup): JavaTargetGroup {
    return {
        loader: group.loader,
        classes: new Map<JavaClassName, JavaTargetClass>(
            group.classes.map(klass => [klass.name, javaTargetClassFromMatchClass(klass)]))
    };
}

function javaTargetClassFromMatchClass(klass: _Java.EnumerateMethodsMatchClass): JavaTargetClass {
    return {
        methods: new Map<JavaMethodName, JavaMethodNameOrSignature>(
            klass.methods.map(fullName => [javaBareMethodNameFromMethodName(fullName), fullName]))
    };
}

function javaBareMethodNameFromMethodName(fullName: string) {
    const signatureStart = fullName.indexOf("(");
    return (signatureStart === -1) ? fullName : fullName.substr(0, signatureStart);
}

function find<T>(array: T[], predicate: (candidate: T) => boolean): T | undefined {
    for (const element of array) {
        if (predicate(element)) {
            return element;
        }
    }
}

function noop() {
}

function registerLazyBridgeGetter(name: string) {
    Object.defineProperty(globalThis, name, {
        enumerable: true,
        configurable: true,
        get() {
            return lazyLoadBridge(name);
        }
    });
}

function lazyLoadBridge(name: string): unknown {
    send({ type: "frida:load-bridge", name });
    let bridge: unknown;
    recv("frida:bridge-loaded", message => {
        bridge = Script.evaluate(`/frida/bridges/${message.filename}`,
            "(function () { " + [
                message.source,
                `Object.defineProperty(globalThis, '${name}', { value: bridge });`,
                `return bridge;`
            ].join("\n") + " })();");
    }).wait();
    return bridge;
}

declare global {
    var stage: Stage;
    var parameters: TraceParameters;
    var state: TraceState;
    var defineHandler: (handler: TraceScriptHandler) => TraceScriptHandler;

    var Java: typeof _Java;
}

interface TraceScriptHandler {
    onEnter?(this: InvocationContext, log: TraceLogFunction, args: InvocationArguments, state: TraceScriptState): void;
    onLeave?(this: InvocationContext, log: TraceLogFunction, retval: InvocationReturnValue, state: TraceScriptState): void;
}

type TraceLogFunction = (...args: any[]) => void;

interface TraceScriptState {
    [x: string]: any;
}

type Stage = "early" | "late";

interface TraceParameters {
    [name: string]: any;
}

interface TraceState {
    [name: string]: any;
}

interface InitScript {
    filename: string;
    source: string;
}

type TraceSpec = TraceSpecItem[];
type TraceSpecItem = [TraceSpecOperation, TraceSpecScope, TraceSpecPattern];
type TraceSpecOperation = "include" | "exclude";
type TraceSpecScope =
    | "module"
    | "function"
    | "relative-function"
    | "absolute-instruction"
    | "imports"
    | "objc-method"
    | "swift-func"
    | "java-method"
    | "debug-symbol"
    ;
type TraceSpecPattern = string;

interface TracePlanRequest {
    plan: TracePlan;
    ready: Promise<void>;
}

class TracePlan {
    native: NativeTargets = new Map<NativeId, NativeTarget>();
    java: JavaTargetGroup[] = [];

    #cachedModules: ModuleMap | null = null;

    get modules(): ModuleMap {
        let modules = this.#cachedModules;
        if (modules === null) {
            modules = new ModuleMap();
            this.#cachedModules = modules;
        }
        return modules;
    }
}

type TargetFlavor = NativeTargetFlavor | "java";
type ScopeName = string;
type MemberName = string | [string, string];

type NativeTargetFlavor = "insn" | "c" | "objc" | "swift";
type NativeTargets = Map<NativeId, NativeTarget>;
type NativeTarget = [type: NativeTargetFlavor, scope: ScopeName, name: MemberName];
type NativeTargetScopes = Map<ScopeName, NativeItem[]>;
type NativeItem = [name: MemberName, address: NativePointer];
type NativeId = string;

interface JavaTargetGroup {
    loader: _Java.Wrapper | null;
    classes: Map<string, JavaTargetClass>;
}
interface JavaTargetClass {
    methods: Map<JavaMethodName, JavaMethodNameOrSignature>;
}
type JavaClassName = string;
type JavaMethodName = string;
type JavaMethodNameOrSignature = string;

type TraceErrorEventHandler = (error: TraceError) => void;
interface TraceError {
    id: TraceTargetId;
    name: string;
    message: string;
}

type StagedItem = [id: StagedItemId, scope: ScopeName, member: MemberName];
type StagedItemId = number;
interface CommitResult {
    ids: TraceTargetId[];
    errors: TraceError[];
}

interface HandlerRequest {
    type: "handlers:get",
    flavor: TargetFlavor;
    baseId: TraceTargetId;
    scopes: HandlerRequestScope[];
}
interface HandlerRequestScope {
    name: string;
    members: MemberName[];
    addresses?: string[];
}
interface HandlerResponse {
    scripts: HandlerScript[];
}
type HandlerScript = string;
interface HandlerConfig {
    muted: boolean;
    capture_backtraces: boolean;
}

type TraceTargetId = number;
type TraceEvent = [TraceTargetId, Timestamp, ThreadId, Depth, Caller, Backtrace, Message];

type Timestamp = number;
type Depth = number;
type Caller = string | null;
type Backtrace = string[] | null;
type Message = string;

type TraceHandler = TraceFunctionHandler | TraceInstructionHandler;
type TraceFunctionHandler = [onEnter: TraceEnterHandler, onLeave: TraceLeaveHandler, config: HandlerConfig];
type TraceInstructionHandler = [onHit: TraceProbeHandler, config: HandlerConfig];
type TraceEnterHandler = (log: LogHandler, args: any[], state: TraceState) => void;
type TraceLeaveHandler = (log: LogHandler, retval: any, state: TraceState) => any;
type TraceProbeHandler = (log: LogHandler, args: any[], state: TraceState) => void;

type CutPoint = ">" | "|" | "<";

type LogHandler = (...message: string[]) => void;

const agent = new Agent();

rpc.exports = {
    init: agent.init.bind(agent),
    dispose: agent.dispose.bind(agent),
    updateHandlerCode: agent.updateHandlerCode.bind(agent),
    updateHandlerConfig: agent.updateHandlerConfig.bind(agent),
    stageTargets: agent.stageTargets.bind(agent),
    commitTargets: agent.commitTargets.bind(agent),
    readMemory: agent.readMemory.bind(agent),
    resolveAddresses: agent.resolveAddresses.bind(agent),
};
