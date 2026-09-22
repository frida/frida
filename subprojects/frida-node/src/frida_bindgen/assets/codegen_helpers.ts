type SignalTransformer<
    Source extends SignalHandler,
    Target extends SignalHandler
> = (...args: Parameters<Source>) => Parameters<Target>;

type SignalInterceptor<H extends SignalHandler> = (...args: Parameters<H>) => boolean;

interface SignalWrapperOptionsNoTransform<H extends SignalHandler> {
    transform?: undefined;
    intercept?: SignalInterceptor<H>;
}

interface SignalWrapperOptionsTransform<
    Source extends SignalHandler,
    Target extends SignalHandler
> {
    transform: SignalTransformer<Source, Target>;
    intercept?: SignalInterceptor<Target>;
}

type SignalWrapperOptions<
    Source extends SignalHandler,
    Target extends SignalHandler
> =
    | SignalWrapperOptionsNoTransform<Source & Target>
    | SignalWrapperOptionsTransform<Source, Target>;

class SignalWrapper<
    SourceHandler extends SignalHandler,
    TargetHandler extends SignalHandler
> {
    #source: Signal<SourceHandler>;
    #transform?: SignalTransformer<SourceHandler, TargetHandler>;
    #intercept?: SignalInterceptor<any>;

    #handlers = new Set<TargetHandler>();

    constructor(
        source: Signal<SourceHandler>,
        options?: SignalWrapperOptions<SourceHandler, TargetHandler>
    ) {
        this.#source = source;

        if (options === undefined || options.transform === undefined) {
            this.#intercept = options?.intercept;
        } else {
            this.#transform = options.transform;
            this.#intercept = options.intercept;
        }
    }

    connect(handler: TargetHandler): void {
        this.#handlers.add(handler);
        if (this.#handlers.size === 1) {
            this.#source.connect(this.#wrappedHandler);
        }
    }

    disconnect(handler: TargetHandler): void {
        this.#handlers.delete(handler);
        if (this.#handlers.size === 0) {
            this.#source.disconnect(this.#wrappedHandler);
        }
    }

    #wrappedHandler = ((...sourceArgs: Parameters<SourceHandler>) => {
        let targetArgs: Parameters<TargetHandler>;
        const transform = this.#transform;
        if (transform === undefined) {
            targetArgs = sourceArgs as unknown as Parameters<TargetHandler>;
        } else {
            targetArgs = transform(...sourceArgs);
        }

        const intercept = this.#intercept;
        if (intercept !== undefined) {
            if (!intercept(...targetArgs)) {
                return;
            }
        }

        for (const handler of this.#handlers) {
            handler(...targetArgs);
        }
    }) as SourceHandler;
}

function inspectWrapper(object: any, name: string, properties: string[], depth: number, options: util.InspectOptionsStylized): string {
    if (depth < 0) {
        return options.stylize(`[${name}]`, "special");
    }

    const summary = Object.fromEntries(properties.map(name => [name, object[name]]));

    const nextOptions = Object.assign({}, options, {
        depth: (options.depth === null) ? null : depth - 1
    });

    return name + " " + inspect(summary, nextOptions);
}
