import "./App.css";
import AddTargetsDialog from "./AddTargetsDialog.tsx";
import DisassemblyView from "./DisassemblyView.tsx";
import EventView from "./EventView.tsx";
import HandlerEditor from "./HandlerEditor.tsx";
import HandlerList from "./HandlerList.tsx";
import MemoryView from "./MemoryView.tsx";
import { useModel } from "./model.js";
import {
    BlueprintProvider,
    Callout,
    Button,
    ButtonGroup,
    Switch,
    Tabs,
    Tab,
} from "@blueprintjs/core";
import { Resplit } from "react-resplit";

export default function App() {
    const {
        lostConnection,

        spawnedProgram,
        respawn,

        handlers,
        selectedScope,
        selectScope,
        selectedHandler,
        setSelectedHandlerId,
        handlerCode,
        draftedCode,
        setDraftedCode,
        deployCode,
        handlerMuted,
        setHandlerMuted,
        captureBacktraces,
        setCaptureBacktraces,

        selectedTabId,
        setSelectedTabId,

        events,
        latestMatchingEventIndex,
        selectedEventIndex,
        setSelectedEventIndex,

        disassemblyTarget,
        disassemble,

        memoryLocation,
        showMemoryLocation,

        addingTargets,
        startAddingTargets,
        finishAddingTargets,
        stageItems,
        stagedItems,
        commitItems,

        addInstructionHook,

        symbolicate,
    } = useModel();

    const connectionError = lostConnection
        ? <Callout
            title="Lost connection to frida-trace"
            intent="danger"
        />
        : null;

    const eventView = (
        <EventView
            events={events}
            selectedIndex={selectedEventIndex}
            onActivate={(handlerId, eventIndex) => {
                setSelectedHandlerId(handlerId);
                setSelectedEventIndex(eventIndex);
            }}
            onDeactivate={() => {
                setSelectedEventIndex(null);
            }}
            onDisassemble={disassemble}
            onSymbolicate={symbolicate}
        />
    );

    const disassemblyView = (
        <DisassemblyView
            target={disassemblyTarget}
            handlers={handlers}
            onSelectTarget={disassemble}
            onSelectHandler={setSelectedHandlerId}
            onSelectMemoryLocation={showMemoryLocation}
            onAddInstructionHook={addInstructionHook}
            onSymbolicate={symbolicate}
        />
    );

    const memoryView = (
        <MemoryView
            address={memoryLocation}
            onSelectAddress={showMemoryLocation}
        />
    );

    return (
        <>
            <Resplit.Root className="app-content" direction="vertical">
                <Resplit.Pane className="top-area" order={0} initialSize="0.5fr">
                    <section className="navigation-area">
                        <HandlerList
                            handlers={handlers}
                            selectedScope={selectedScope}
                            onScopeSelect={selectScope}
                            selectedHandler={selectedHandler?.id ?? null}
                            onHandlerSelect={setSelectedHandlerId}
                        />
                        <ButtonGroup className="target-actions" vertical={true} minimal={true} alignText="left">
                            <Button intent="success" icon="add" onClick={startAddingTargets}>Add</Button>
                            {(spawnedProgram !== null) ? <Button intent="danger" icon="reset" onClick={respawn}>Respawn</Button> : null}
                        </ButtonGroup>
                    </section>
                    <section className="editor-area">
                        {connectionError}
                        <section className="editor-toolbar">
                            <ButtonGroup minimal={true}>
                                <Button
                                    icon="rocket-slant"
                                    disabled={draftedCode === handlerCode}
                                    onClick={() => deployCode(draftedCode)}
                                >
                                    Deploy
                                </Button>
                                <Button
                                    icon="arrow-down"
                                    disabled={latestMatchingEventIndex === null}
                                    onClick={() => setSelectedEventIndex(latestMatchingEventIndex)}
                                >
                                    Latest Event
                                </Button>
                                <Button
                                    icon="code"
                                    disabled={lostConnection || selectedHandler === null || selectedHandler.address === null}
                                    onClick={() => {
                                        disassemble({
                                            type: (selectedHandler!.flavor === "insn") ? "instruction" : "function",
                                            name: selectedHandler!.display_name,
                                            address: BigInt(selectedHandler!.address!)
                                        });
                                    }}
                                >
                                    Disassemble
                                </Button>
                            </ButtonGroup>
                            <div>
                                <Switch
                                    className="handler-muted"
                                    inline={true}
                                    checked={handlerMuted}
                                    onChange={e => setHandlerMuted(e.target.checked)}
                                >
                                    Muted
                                </Switch>
                                <Switch
                                    inline={true}
                                    checked={captureBacktraces}
                                    onChange={e => setCaptureBacktraces(e.target.checked)}
                                >
                                    Capture Backtraces
                                </Switch>
                            </div>
                        </section>
                        <HandlerEditor
                            handlerId={selectedHandler?.id ?? null}
                            handlerCode={handlerCode}
                            onChange={setDraftedCode}
                            onSave={deployCode}
                        />
                    </section>
                </Resplit.Pane>
                <Resplit.Splitter className="app-splitter" order={1} size="5px" />
                <Resplit.Pane className="bottom-area" order={2} initialSize="0.5fr">
                    <Tabs className="bottom-tabs" selectedTabId={selectedTabId} onChange={tabId => setSelectedTabId(tabId as any)} animate={false}>
                        <Tab id="events" title="Events" panel={eventView} panelClassName="bottom-tab-panel" />
                        <Tab id="disassembly" title="Disassembly" panel={disassemblyView} panelClassName="bottom-tab-panel" />
                        <Tab id="memory" title="Memory" panel={memoryView} panelClassName="bottom-tab-panel" />
                    </Tabs>
                </Resplit.Pane>
            </Resplit.Root>
            <AddTargetsDialog
                isOpen={addingTargets}
                stagedItems={stagedItems}
                onClose={finishAddingTargets}
                onQuery={stageItems}
                onCommit={commitItems}
            />
            <BlueprintProvider>
                <div />
            </BlueprintProvider>
        </>
    );
}
