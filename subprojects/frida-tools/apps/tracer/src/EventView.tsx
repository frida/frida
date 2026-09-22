import "./EventView.css";
import { DisassemblyTarget, Event, HandlerId } from "./model.js";
import { Button, Card } from "@blueprintjs/core";
import Ansi from "@curvenote/ansi-to-react";
import prettyMilliseconds from "pretty-ms";
import { ReactElement, useCallback, useEffect, useRef, useState } from "react";
import AutoSizer from "react-virtualized-auto-sizer";
import { VariableSizeList } from "react-window";

export interface EventViewProps {
    events: Event[];
    selectedIndex: number | null;
    onActivate: EventActionHandler;
    onDeactivate: EventActionHandler;
    onDisassemble: DisassembleHandler;
    onSymbolicate: SymbolicateHandler;
}

export type EventActionHandler = (handlerId: HandlerId, eventIndex: number) => void;
export type DisassembleHandler = (target: DisassemblyTarget) => void;
export type SymbolicateHandler = (addresses: bigint[]) => Promise<string[]>;

const NON_BLOCKING_SPACE = "\u00A0";
const INDENT = NON_BLOCKING_SPACE.repeat(3) + "|" + NON_BLOCKING_SPACE;

export default function EventView({
    events,
    selectedIndex = null,
    onActivate,
    onDeactivate,
    onDisassemble,
    onSymbolicate,
}: EventViewProps) {
    const listRef = useRef<VariableSizeList>(null);
    const listOuterRef = useRef<HTMLElement>(null);
    const [items, setItems] = useState<Item[]>([]);
    const [selectedCallerSymbol, setSelectedCallerSymbol] = useState<string | null>("");
    const [selectedBacktraceSymbols, setSelectedBacktraceSymbols] = useState<string[] | null>(null);
    const autoscroll = useRef<AutoscrollState>({ enabled: true, pending: [] });

    useEffect(() => {
        let lastTimestamp: number | null = null;
        let lastTid: number | null = null;
        const newItems = events.reduce((result, event, i) => {
            const ev = event.slice() as Event;
            const [_targetId, timestamp, threadId, _depth, _caller, _backtrace, _message, style] = ev;

            if (lastTimestamp !== null) {
                ev[1] = timestamp - lastTimestamp;
            } else {
                ev[1] = 0;
            }
            lastTimestamp = timestamp;

            if (threadId !== lastTid) {
                result.push([i, threadId, style]);
                lastTid = threadId;
            }

            result.push([i, ev]);
            return result;
        }, [[] as SpacerItem] as Item[]);
        setItems(newItems);

        let ignore = false;

        if (autoscroll.current.enabled) {
            autoscroll.current.pending.push(() => {
                if (!ignore) {
                    listRef.current?.scrollToItem(newItems.length - 1, "end");
                }
            });
        }

        return () => {
            ignore = true;
        };
    }, [events]);

    const itemSize = useCallback((i: number) => {
        const item = items[i];

        if (item.length === 0) {
            return 5;
        }

        if (item.length === 3) {
            return 15.43;
        }

        const [eventIndex, event] = item;
        const [_targetId, _timestamp, _threadId, _depth, _caller, backtrace, message, _style] = event;
        const numLines = message.split("\n").length;
        let size = 30;
        if (numLines > 1) {
            size += (numLines - 1) * 14;
            size -= 7;
        }
        if (eventIndex === selectedIndex) {
            size += 150;
            if (backtrace !== null) {
                size += (backtrace.length - 1) * 34;
            }
        }
        return size;
    }, [items, selectedIndex]);

    useEffect(() => {
        setSelectedCallerSymbol(null);
        setSelectedBacktraceSymbols(null);

        const list = listRef.current;
        if (list !== null) {
            list.resetAfterIndex(0, true);
            if (selectedIndex !== null) {
                const itemIndex = items.findIndex(([i,]) => i === selectedIndex);
                list.scrollToItem(itemIndex);
            }
        }
    }, [selectedIndex]);

    useEffect(() => {
        if (selectedIndex === null) {
            return;
        }

        const [_targetId, _timestamp, _threadId, _depth, caller, backtrace, _message, _style] = events[selectedIndex];
        let ignore = false;

        async function symbolicate() {
            if (caller !== null && backtrace === null) {
                const [symbol] = await onSymbolicate([BigInt(caller)]);
                if (!ignore) {
                    setSelectedCallerSymbol(symbol);
                }
            }

            if (backtrace !== null) {
                const symbols = await onSymbolicate(backtrace.map(BigInt));
                if (!ignore) {
                    setSelectedBacktraceSymbols(symbols);
                }
            }
        }

        symbolicate();

        return () => {
            ignore = true;
        };
    }, [events, selectedIndex, onSymbolicate]);

    const handleDisassemblyRequest = useCallback((rawAddress: string) => {
        onDisassemble({ type: "instruction", address: BigInt(rawAddress) });
    }, [onDisassemble]);

    return (
        <AutoSizer className="event-view">
            {({ width, height }) => (
                <VariableSizeList<Item[]>
                    ref={listRef}
                    outerRef={listOuterRef}
                    className="event-list"
                    width={width}
                    height={height}
                    itemCount={items.length}
                    itemSize={itemSize}
                    itemData={items}
                    onItemsRendered={() => {
                        let work: AutoscrollWork | undefined;
                        while ((work = autoscroll.current.pending.shift()) !== undefined) {
                            work();
                        }
                    }}
                    onScroll={props => {
                        if (!props.scrollUpdateWasRequested) {
                            const container = listOuterRef.current!;
                            autoscroll.current.enabled = container.scrollTop >= (container.scrollHeight - container.offsetHeight - 20);
                        }
                    }}
                >
                    {({ data, index: itemIndex, style }) => {
                        const item = data[itemIndex];

                        if (item.length === 0) {
                            return (
                                <div className="event-spacer" style={style} />
                            );
                        }

                        if (item.length === 3) {
                            const [, threadId, textStyle] = item;
                            const colorClass = "ansi-" + textStyle.join("-");
                            return (
                                <div
                                    className={"event-heading " + colorClass}
                                    style={{ height: itemSize(itemIndex), ...style }}
                                >
                                    /* TID 0x{threadId.toString(16)} */
                                </div>
                            );
                        }

                        const [eventIndex, event] = item;
                        const [targetId, timestamp, threadId, depth, caller, backtrace, message, textStyle] = event;

                        const isSelected = eventIndex === selectedIndex;
                        let selectedEventDetails: ReactElement | undefined;
                        if (isSelected) {                    
                            selectedEventDetails = (
                                <Card className="event-details" interactive={true} compact={true}>
                                    <table>
                                        <tbody>
                                            <tr>
                                                <td>Thread ID</td>
                                                <td>0x{threadId.toString(16)}</td>
                                                <td>
                                                </td>
                                            </tr>
                                            {(caller !== null && backtrace === null) ? (
                                                <tr>
                                                    <td>Caller</td>
                                                    <td>
                                                        <Button onClick={() => handleDisassemblyRequest(caller)}>{selectedCallerSymbol ?? caller}</Button>
                                                    </td>
                                                </tr>
                                            ) : null
                                            }
                                            {(backtrace !== null) ? (
                                                <tr>
                                                    <td>Backtrace</td>
                                                    <td>
                                                        {backtrace.map((address, i) =>
                                                            <Button key={address} alignText="left" onClick={() => handleDisassemblyRequest(address)}>
                                                            {(selectedBacktraceSymbols !== null) ? selectedBacktraceSymbols[i] : address}
                                                        </Button>)}
                                                    </td>
                                                </tr>
                                            ) : null
                                            }
                                        </tbody>
                                    </table>
                                    <Button className="event-dismiss" intent="primary" onClick={() => onDeactivate(targetId, eventIndex)}>Dismiss</Button>
                                </Card>
                            );
                        }
                    
                        const eventClasses = ["event-item"];
                        if (isSelected) {
                            eventClasses.push("event-selected");
                        }

                        const timestampStr = (timestamp !== 0) ? `+${prettyMilliseconds(timestamp)}` : "";

                        const colorClass = "ansi-" + textStyle.join("-");

                        return (
                            <div
                                className={eventClasses.join(" ")}
                                style={{ height: itemSize(itemIndex), ...style }}
                            >
                                <div className="event-summary">
                                    <span className="event-timestamp">{timestampStr}</span>
                                    <span className={"event-indent " + colorClass}>{INDENT.repeat(depth)}</span>
                                    <Button
                                        className={"event-message " + colorClass}
                                        minimal={true}
                                        alignText="left"
                                        onClick={() => onActivate(targetId, eventIndex)}
                                    >
                                        <Ansi>{message}</Ansi>
                                    </Button>
                                </div>
                                {isSelected ? selectedEventDetails : null}
                            </div>
                        );
                    }}
                </VariableSizeList>
            )}
        </AutoSizer>
    );
}

type Item = EventItem | ThreadIdMarkerItem | SpacerItem;
type EventItem = [index: number, event: Event];
type ThreadIdMarkerItem = [index: number, threadId: number, style: string[]];
type SpacerItem = [];

interface AutoscrollState {
    enabled: boolean;
    pending: AutoscrollWork[];
}

type AutoscrollWork = () => void;
