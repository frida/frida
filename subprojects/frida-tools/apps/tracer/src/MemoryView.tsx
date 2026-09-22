import "./MemoryView.css";
import { Button, ControlGroup, InputGroup, SegmentedControl, Spinner } from "@blueprintjs/core";
import { useR2 } from "@frida/react-use-r2";
import { useCallback, useEffect, useRef, useState } from "react";

export interface MemoryViewProps {
    address?: bigint;
    onSelectAddress: SelectAddressHandler;
}

export type SelectAddressHandler = (address: bigint) => void;

export default function MemoryView({ address, onSelectAddress }: MemoryViewProps) {
    const addressInputRef = useRef<HTMLInputElement>(null);
    const [format, setFormat] = useState("x");
    const [data, setData] = useState("");
    const [isLoading, setIsLoading] = useState(false);
    const { executeR2Command } = useR2();

    useEffect(() => {
        if (address === undefined) {
            return;
        }

        let ignore = false;
        setIsLoading(true);

        async function start() {
            const data = await executeR2Command(`${format} @ 0x${address!.toString(16)}`);
            if (ignore) {
                return;
            }

            setData(data);
            setIsLoading(false);
        }

        start();

        return () => {
            ignore = true;
        };
    }, [format, address, executeR2Command]);

    useEffect(() => {
        if (address === undefined) {
            return;
        }

        addressInputRef.current!.value = `0x${address.toString(16)}`;
    }, [address]);

    const adjustAddress = useCallback((delta: number) => {
        let newAddress: bigint;
        try {
            newAddress = BigInt(addressInputRef.current!.value) + BigInt(delta);
        } catch (e) {
            return;
        }
        onSelectAddress(newAddress);
    }, [onSelectAddress]);

    if (isLoading) {
        return (
            <Spinner className="memory-view" />
        );
    }

    return (
        <div className="memory-view">
            <div className="memory-view-toolbar">
                <ControlGroup>
                    <Button icon="arrow-left" onClick={() => adjustAddress(-16)}></Button>
                    <InputGroup
                        inputRef={addressInputRef}
                        onKeyDown={e => {
                            if (e.key === "Enter") {
                                e.preventDefault();
                                adjustAddress(0);
                            }
                        }}
                        placeholder="Memory address…"
                    />
                    <Button icon="arrow-right" onClick={() => adjustAddress(16)}></Button>
                </ControlGroup>
                <SegmentedControl
                    small={true}
                    options={[
                        {
                            label: "Raw",
                            value: "x",
                        },
                        {
                            label: "64",
                            value: "pxq"
                        },
                        {
                            label: "32",
                            value: "pxw"
                        },
                        {
                            label: "Periscope",
                            value: "pxr 4K"
                        },
                    ]}
                    defaultValue="x"
                    onValueChange={setFormat}
                />
            </div>
            <div className="memory-view-data" dangerouslySetInnerHTML={{ __html: data }} />
        </div>
    );
}
