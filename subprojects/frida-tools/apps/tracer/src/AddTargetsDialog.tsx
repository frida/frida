import "./AddTargetsDialog.css";
import { TraceSpecScope, StagedItem, StagedItemId } from "./model.js";
import {
    Button,
    ButtonGroup,
    Card,
    CardList,
    Dialog,
    DialogBody,
    DialogFooter,
    FormGroup,
    InputGroup,
    Menu,
    MenuItem,
    Popover
} from "@blueprintjs/core";
import { useRef, useState } from "react";
import { useDebouncedCallback } from "use-debounce";

export interface AddTargetsProps {
    isOpen: boolean;
    stagedItems: StagedItem[];
    onClose: CloseEventHandler;
    onQuery: QueryEventHandler;
    onCommit: CommitEventHandler;
}

export type CloseEventHandler = () => void;
export type QueryEventHandler = (scope: TraceSpecScope, query: string) => void;
export type CommitEventHandler = (id: StagedItemId | null) => void;

export default function AddTargetsDialog({ isOpen, stagedItems, onClose, onQuery, onCommit }: AddTargetsProps) {
    const inputRef = useRef<HTMLInputElement>(null);
    const onQueryDebounced = useDebouncedCallback(onQuery, 250);
    const [scope, setScope] = useState(TraceSpecScope.Function);

    const kindMenu = (
        <Popover
            content={
                <Menu>
                    {
                        Object.keys(TraceSpecScope)
                            .map(s => (
                                <MenuItem
                                    key={s}
                                    text={labelForTraceSpecScope((TraceSpecScope as any)[s])}
                                    onClick={() => setScope((TraceSpecScope as any)[s])}
                                />
                            ))
                    }
                </Menu>
            }
            placement="bottom-end"
        >
            <Button minimal={true} rightIcon="caret-down">
                {labelForTraceSpecScope(scope)}
            </Button>
        </Popover>
    );

    const actions = (
        <ButtonGroup>
            <Button onClick={onClose}>Close</Button>
            <Button type="submit" intent="primary" disabled={stagedItems.length === 0} onClick={() => onCommit(null)}>Add All</Button>
        </ButtonGroup>
    );

    const candidates = (stagedItems.length !== 0) ? (
        <FormGroup>
            <CardList compact={true}>
                {stagedItems.map(([id, scope, member]) => {
                    return (
                        <Card key={id} className="staged-item" interactive={false}>
                            <span>{scope}!{member}</span>
                            <Button minimal={true} intent="primary" small={true} text="Add" onClick={() => onCommit(id)} />
                        </Card>
                    );
                })}
            </CardList>
        </FormGroup>
    ) : null;

    return (
        <Dialog title="Add targets" isOpen={isOpen} onOpened={() => inputRef.current?.focus()} onClose={onClose}>
            <form onSubmit={e => e.preventDefault()}>
                <DialogBody>
                    <FormGroup>
                        <InputGroup
                            inputRef={inputRef}
                            placeholder={placeholderForTraceTargetSpecScope(scope)}
                            rightElement={kindMenu}
                            onValueChange={query => onQueryDebounced(scope, query)}
                        />
                    </FormGroup>
                    {candidates}
                </DialogBody>
                <DialogFooter actions={actions} />
            </form>
        </Dialog>
    );
}

function labelForTraceSpecScope(scope: TraceSpecScope) {
    switch (scope) {
        case TraceSpecScope.Function:
            return "Function";
        case TraceSpecScope.RelativeFunction:
            return "Relative Function";
        case TraceSpecScope.AbsoluteInstruction:
            return "Instruction";
        case TraceSpecScope.Imports:
            return "All Module Imports";
        case TraceSpecScope.Module:
            return "All Module Exports";
        case TraceSpecScope.ObjcMethod:
            return "Objective-C Method";
        case TraceSpecScope.SwiftFunc:
            return "Swift Function";
        case TraceSpecScope.JavaMethod:
            return "Java Method";
        case TraceSpecScope.DebugSymbol:
            return "Debug Symbol";
    }
}

function placeholderForTraceTargetSpecScope(scope: TraceSpecScope) {
    switch (scope) {
        case TraceSpecScope.Function:
            return "[Module!]Function";
        case TraceSpecScope.RelativeFunction:
            return "Module!Offset";
        case TraceSpecScope.AbsoluteInstruction:
            return "0x1234";
        case TraceSpecScope.Imports:
        case TraceSpecScope.Module:
            return "Module";
        case TraceSpecScope.ObjcMethod:
            return "-[*Auth foo:bar:], +[Foo foo*], or *[Bar baz]";
        case TraceSpecScope.SwiftFunc:
            return "*SomeModule*!SomeClassPrefix*.*secret*()";
        case TraceSpecScope.JavaMethod:
            return "Class!Method";
        case TraceSpecScope.DebugSymbol:
            return "Function";
    }
}