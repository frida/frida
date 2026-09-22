function get_device_arguments
	set tokens (commandline --tokenize --current-process)
	set device_argument_flags "-D" "--device" "-H" "--host" "--certificate" "--origin" "--token" "--stun-server" "--relay"
	set device_flags "-U" "--usb" "-R" "--remote" "--p2p"

	for idx in (seq (count $tokens))
		if contains -- "$tokens[$idx]" $device_argument_flags
			echo "$tokens[$idx]"
			if not contains "=" "$tokens[$idx]"
				set next_idx (math $idx + 1)
				echo "$tokens[$next_idx]"
			end
		else if contains -- "$tokens[$idx]" $device_flags
			echo "$tokens[$idx]"
		end
	end
end

function get_device_processes
	set tmpout (mktemp)
	set relevant_flags (get_device_arguments)

	frida-ps $relevant_flags 2>/dev/null > "$tmpout"
	cat "$tmpout" | awk 'NR>2 {print $1"\t"$2}' | sort --numeric-sort
	cat "$tmpout" | awk 'NR>2 {print $2"\t"$1}' | sort --numeric-sort --key=2,2
	rm --force $tmpout
end

function get_device_pids
	set tmpout (mktemp)
	set relevant_flags (get_device_arguments)

	frida-ps $relevant_flags 2>/dev/null >"$tmpout"
	cat "$tmpout" | awk 'NR>2 {print $1"\t"$2}' | sort --numeric-sort
	rm --force $tmpout
end

function get_device_processes_names
	set tmpout (mktemp)
	set relevant_flags (get_device_arguments)

	frida-ps $relevant_flags 2>/dev/null > "$tmpout"
	cat "$tmpout" | awk 'NR>2 {print $2"\t"$1}' | sort --numeric-sort --key=2,2
	rm --force $tmpout
end

function get_device_identifiers
	set tmpout (mktemp)
	set relevant_flags (get_device_arguments)

	frida-ps --applications $relevant_flags 2>/dev/null > "$tmpout"
	cat "$tmpout" | awk 'NR>2 {print $3"\t"$1}' | sort --numeric-sort --key=2,2
	rm --force $tmpout
end

function get_frida_devices
	frida-ls-devices | tail -n +3 | awk '{print $1"\t"substr($0, index($0,$3))}'
end

function add_base_arguments
	complete --command "$argv[1]" --force-files --require-parameter --short-option=O --long-option=options-file --description="text file containing additional command line options"
	complete --command "$argv[1]" --no-files --long-option=version --description="show program's version number and exit"
	complete --command "$argv[1]" --no-files --short-option=h --long-option=help --description="show this help message and exit"
end

function add_device_arguments
	complete --command "$argv[1]" --no-files --require-parameter --short-option=D --long-option=device --description="connect to device with the given ID" --arguments="(get_frida_devices)"
	complete --command "$argv[1]" --no-files --short-option=U --long-option=usb --description="connect to USB device"
	complete --command "$argv[1]" --no-files --short-option=R --long-option=remote --description="connect to remote frida-server"
	complete --command "$argv[1]" --no-files --require-parameter --short-option=H --long-option=host --description="connect to remote frida-server on HOST"
	complete --command "$argv[1]" --force-files --require-parameter --long-option=certificate --description="speak TLS with HOST, expecting CERTIFICATE"
	complete --command "$argv[1]" --no-files --require-parameter --long-option=origin --description="connect to remote server with “Origin” header set to ORIGIN"
	complete --command "$argv[1]" --no-files --require-parameter --long-option=token --description="authenticate with HOST using TOKEN"
	complete --command "$argv[1]" --no-files --require-parameter --long-option=keepalive-interval --description="set keepalive interval in seconds, or 0 to disable (defaults to -1 to auto-select based on transport)"
	complete --command "$argv[1]" --no-files --long-option=p2p --description="establish a peer-to-peer connection with target"
	complete --command "$argv[1]" --no-files --require-parameter --long-option=stun-server --description="set STUN server ADDRESS to use with --p2p"
	complete --command "$argv[1]" --no-files --require-parameter --long-option=relay --description="add relay to use with --p2p"
end

function add_target_arguments
	complete --command "$argv[1]" --no-files --require-parameter --short-option=f --long-option=file --description="Spawn FILE"
	complete --command "$argv[1]" --no-files --short-option=F --long-option=attach-frontmost --description="attach to frontmost application"
	complete --command "$argv[1]" --no-files --require-parameter --short-option=n --long-option=attach-name --description="attach to NAME" --arguments="(get_device_processes_names)"
	complete --command "$argv[1]" --no-files --require-parameter --short-option=N --long-option=attach-identifier --description="attach to IDENTIFIER" --arguments="(get_device_identifiers)"
	complete --command "$argv[1]" --no-files --require-parameter --short-option=p --long-option=attach-pid --description="attach to PID" --arguments="(get_device_pids)"
	complete --command "$argv[1]" --no-files --require-parameter --short-option=W --long-option=await --description="await spawn matching PATTERN"
	complete --command "$argv[1]" --no-files --require-parameter --long-option=stdio --description="stdio behavior when spawning" --arguments="inherit pipe"
	complete --command "$argv[1]" --no-files --require-parameter --long-option=aux --description="set aux option when spawning"
	complete --command "$argv[1]" --no-files --require-parameter --long-option=realm --description="realm to attach in" --arguments="native emulated"
	complete --command "$argv[1]" --no-files --require-parameter --long-option=runtime --description="script runtime to use" --arguments="qjs v8"
	complete --command "$argv[1]" --no-files --long-option=debug --description="enable the Node.js compatible script debugger"
	complete --command "$argv[1]" --no-files --long-option=squelch-crash --description="if enabled, will not dump crash report to console"
	complete --command "$argv[1]" --force-files --arguments "(get_device_processes)"
end


######## frida ########
add_base_arguments frida
add_device_arguments frida
add_target_arguments frida
complete --command frida --force-files --require-parameter --short-option=l --long-option=load --description="load SCRIPT"
complete --command frida --no-files --require-parameter --short-option=P --long-option=parameters --description="parameters as JSON, same as Gadget"
complete --command frida --no-files --require-parameter --short-option=C --long-option=cmodule --description="load CMODULE"
complete --command frida --no-files --require-parameter --long-option=toolchain --description="CModule toolchain to use when compiling from source code" --arguments="any internal external"
complete --command frida --no-files --require-parameter --short-option=c --long-option=codeshare --description="load CODESHARE_URI"
complete --command frida --no-files --require-parameter --short-option=e --long-option=eval --description="evaluate CODE"
complete --command frida --no-files --short-option=q --description="quiet mode (no prompt) and quit after -l and -e"
complete --command frida --no-files --require-parameter --short-option=t --long-option=timeout --description="seconds to wait before terminating in quiet mode"
complete --command frida --no-files --long-option=pause --description="leave main thread paused after spawning program"
complete --command frida --force-files --require-parameter --short-option=o --long-option=output --description="output to log file"
complete --command frida --no-files --long-option=eternalize --description="eternalize the script before exit"
complete --command frida --no-files --long-option=exit-on-error --description="exit with code 1 after encountering any exception in the SCRIPT"
complete --command frida --no-files --long-option=auto-perform --description="wrap entered code with Java.perform"
complete --command frida --no-files --long-option=auto-reload --description="Enable auto reload of provided scripts and c module (on by default, will be required in the future)"
complete --command frida --no-files --long-option=no-auto-reload --description="Disable auto reload of provided scripts and c module"


######## frida-ls-devices ########
add_base_arguments frida-ls-devices


######## frida-ps ########
add_base_arguments frida-ps
add_device_arguments frida-ps
complete --command frida-ps --no-files --short-option=a --long-option=applications --description="list only applications"
complete --command frida-ps --no-files --short-option=i --long-option=installed --description="include all installed applications"
complete --command frida-ps --no-files --short-option=j --long-option=json --description="output results as JSON"


######## frida-kill ########
add_base_arguments frida-kill
add_device_arguments frida-kill
complete --command frida-kill --no-files --arguments "(get_device_processes)"


######## frida-discover ########
add_base_arguments frida-discover
add_device_arguments frida-discover
add_target_arguments frida-discover


######## frida-trace ########
add_base_arguments frida-trace
add_device_arguments frida-trace
add_target_arguments frida-trace
complete --command frida-trace --no-files --require-parameter --short-option=I --long-option=include-module --description="include MODULE"
complete --command frida-trace --no-files --require-parameter --short-option=X --long-option=exclude-module --description="exclude MODULE"
complete --command frida-trace --no-files --require-parameter --short-option=i --long-option=include --description="include [MODULE!]FUNCTION"
complete --command frida-trace --no-files --require-parameter --short-option=x --long-option=exclude --description="exclude [MODULE!]FUNCTION"
complete --command frida-trace --no-files --require-parameter --short-option=a --long-option=add --description="add MODULE!OFFSET"
complete --command frida-trace --no-files --require-parameter --short-option=T --long-option=include-imports --description="include program's imports"
complete --command frida-trace --no-files --require-parameter --short-option=t --long-option=include-module-imports --description="include MODULE imports"
complete --command frida-trace --no-files --require-parameter --short-option=m --long-option=include-objc-method --description="include OBJC_METHOD"
complete --command frida-trace --no-files --require-parameter --short-option=M --long-option=exclude-objc-method --description="exclude OBJC_METHOD"
complete --command frida-trace --no-files --require-parameter --short-option=j --long-option=include-java-method --description="include JAVA_METHOD"
complete --command frida-trace --no-files --require-parameter --short-option=J --long-option=exclude-java-method --description="exclude JAVA_METHOD"
complete --command frida-trace --no-files --require-parameter --short-option=s --long-option=include-debug-symbol --description="include DEBUG_SYMBOL"
complete --command frida-trace --no-files --short-option=q --long-option=quiet --description="do not format output messages"
complete --command frida-trace --no-files --short-option=d --long-option=decorate --description="add module name to generated onEnter log statement"
complete --command frida-trace --force-files --require-parameter --short-option=S --long-option=init-session --description="path to JavaScript file used to initialize the session"
complete --command frida-trace --no-files --require-parameter --short-option=P --long-option=parameters --description="parameters as JSON, exposed as a global named 'parameters'"
complete --command frida-trace --force-files --require-parameter --short-option=o --long-option=output --description="dump messages to file"


######## frida-join ########
add_base_arguments frida-join
add_device_arguments frida-join
add_target_arguments frida-join
complete --command frida-join --no-files --require-parameter --long-option=portal-location --description="join portal at LOCATION"
complete --command frida-join --force-files --require-parameter --long-option=portal-certificate --description="speak TLS with portal expecting CERTIFICATE"
complete --command frida-join --no-files --require-parameter --long-option=portal-token --description="authenticate with portal using TOKEN"
complete --command frida-join --no-files --require-parameter --long-option=portal-acl-allow --description="limit portal access to control channels with TAG"
# TODO specify positional arguments better


######## frida-create ########
add_base_arguments frida-create
complete --command frida-create --no-files --short-option=n --long-option=project-name --description="project name"
complete --command frida-create --force-files --short-option=o --long-option=output-directory --description="output directory"
complete --command frida-create --no-files --require-parameter --short-option=t --description="template file" --arguments "agent cmodule"


######## frida-apk ########
add_base_arguments frida-apk
complete --command frida-apk --force-files --short-option=o --long-option=output --description="output path"
complete --command frida-apk --force-files --short-option=g --long-option=gadget --description="inject the specified gadget library"
complete --command frida-apk --force-files --short-option=c --long-option=gadget-config --description="set the given key=value gadget interaction config"


######## frida-compile ########
add_base_arguments frida-compile
complete --command frida-compile --short-option=o --long-option=output --description="write output to <file>" --require-parameter
complete --command frida-compile --short-option=w --long-option=watch --description="watch for changes and recompile"
complete --command frida-compile --short-option=S --long-option=no-source-maps --description="omit source-maps"
complete --command frida-compile --short-option=c --long-option=compress --description="compress using terser"
complete --command frida-compile --short-option=v --long-option=verbose --description="be verbose"
