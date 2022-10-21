Name:						frida-core
Version:				%{_version}
Release:        1%{?dist}
Summary:				Frida core library intended for static linking into bindings 

License:				wxWindows
URL:						https://github.com/frida/frida-core

Requires:				frida-gum

%description
Frida core library intended for static linking into bindings.
* Lets you inject your own JavaScript instrumentation code into other processes, optionally with your own C code for performance-sensitive bits.
* Acts as a logistics layer that packages up GumJS into a shared library.
* Provides a two-way communication channel for talking to your scripts, if needed, and later unload them.
* Also lets you enumerate installed apps, running processes, and connected devices.
* Written in Vala, with OS-specific glue code in C/Objective-C/asm.


%prep


%build
make build/frida_thin-linux-%{_arch}/lib/pkgconfig/frida-core-1.0.pc


%install
mkdir -p %{buildroot}/%{_bindir}
install -m 0755 -D build/frida_thin-linux-%{_arch}/bin/frida-* %{buildroot}/%{_bindir}

mkdir -p %{buildroot}/%{_libdir}
install -m 0644 build/frida_thin-linux-%{_arch}/lib/libfrida-core-1.0.a %{buildroot}/%{_libdir}/libfrida-core-1.0.a
install -m 0755 -d %{buildroot}/%{_libdir}/frida/64
install -m 0644 build/frida_thin-linux-%{_arch}/lib/frida/64/frida-gadget.so %{buildroot}/%{_libdir}/frida/64/frida-gadget.so
install -m 0755 -d %{buildroot}/%{_libdir}/pkgconfig
install -m 0644 build/frida_thin-linux-%{_arch}/lib/pkgconfig/frida-core-1.0.pc %{buildroot}/%{_libdir}/pkgconfig/frida-core-1.0.pc

mkdir -p %{buildroot}/%{_includedir}
install -m 0755 -d %{buildroot}/%{_includedir}/frida-1.0
install -m 0644 build/frida_thin-linux-%{_arch}/include/frida-1.0/frida-core.h %{buildroot}/%{_includedir}/frida-1.0/frida-core.h

mkdir -p %{buildroot}/%{_datadir}
install -m 0755 -d %{buildroot}/%{_datadir}/vala/vapi
install -m 0644 -D build/frida_thin-linux-%{_arch}/share/vala/vapi/frida-core-*.vapi %{buildroot}/%{_datadir}/vala/vapi


%check


%files
%{_bindir}/frida-*
%{_libdir}/libfrida-core-1.0.a
%{_libdir}/frida/64/frida-gadget.so
%{_libdir}/pkgconfig/frida-core-1.0.pc
%{_includedir}/frida-1.0/frida-core.h
%{_datadir}/vala/vapi/frida-core-*.vapi


%changelog

