Name:						frida-gum
Version:				%{_version}
Release:        1%{?dist}
Summary:				Cross-platform instrumentation and introspection library written in C 

License:				wxWindows
URL:						https://github.com/frida/frida-gum

%description
Cross-platform instrumentation and introspection library written in C.
This library is consumed by frida-core through its JavaScript bindings, GumJS.


%prep


%build
make build/frida_thin-linux-%{_arch}/lib/pkgconfig/frida-gum-1.0.pc


%install
mkdir -p %{buildroot}/%{_bindir}
install -m 0755 build/frida_thin-linux-%{_arch}/bin/gum-graft %{buildroot}/%{_bindir}/gum-graft

mkdir -p %{buildroot}/%{_libdir}
install -m 0644 -D build/frida_thin-linux-%{_arch}/lib/libfrida-gum*.a %{buildroot}/%{_libdir}
install -m 0755 build/frida_thin-linux-%{_arch}/lib/libfrida-gumpp-1.0.so %{buildroot}/%{_libdir}/libfrida-gumpp-1.0.so

mkdir -p %{buildroot}/%{_libdir}/pkgconfig
install -m 0644 -D build/frida_thin-linux-%{_arch}/lib/pkgconfig/frida-gum*.pc %{buildroot}/%{_libdir}/pkgconfig

mkdir -p %{buildroot}/%{_includedir}
install -m 0755 -d %{buildroot}/%{_includedir}/frida-1.0/gum
install -m 0644 -D build/frida_thin-linux-%{_arch}/include/frida-1.0/gum/*.h %{buildroot}/%{_includedir}/frida-1.0/gum
install -m 0755 -d %{buildroot}/%{_includedir}/frida-1.0/gum/arch-arm
install -m 0644 -D build/frida_thin-linux-%{_arch}/include/frida-1.0/gum/arch-arm/*.h %{buildroot}/%{_includedir}/frida-1.0/gum/arch-arm
install -m 0755 -d %{buildroot}/%{_includedir}/frida-1.0/gum/arch-arm64
install -m 0644 -D build/frida_thin-linux-%{_arch}/include/frida-1.0/gum/arch-arm64/*.h %{buildroot}/%{_includedir}/frida-1.0/gum/arch-arm64
install -m 0755 -d %{buildroot}/%{_includedir}/frida-1.0/gum/arch-mips
install -m 0644 -D build/frida_thin-linux-%{_arch}/include/frida-1.0/gum/arch-mips/*.h %{buildroot}/%{_includedir}/frida-1.0/gum/arch-mips
install -m 0755 -d %{buildroot}/%{_includedir}/frida-1.0/gum/arch-x86
install -m 0644 -D build/frida_thin-linux-%{_arch}/include/frida-1.0/gum/arch-x86/*.h %{buildroot}/%{_includedir}/frida-1.0/gum/arch-x86
install -m 0755 -d %{buildroot}/%{_includedir}/frida-1.0/gum/heap
install -m 0644 -D build/frida_thin-linux-%{_arch}/include/frida-1.0/gum/heap/*.h %{buildroot}/%{_includedir}/frida-1.0/gum/heap
install -m 0755 -d %{buildroot}/%{_includedir}/frida-1.0/gum/prof
install -m 0644 -D build/frida_thin-linux-%{_arch}/include/frida-1.0/gum/prof/*.h %{buildroot}/%{_includedir}/frida-1.0/gum/prof
install -m 0755 -d %{buildroot}/%{_includedir}/frida-1.0/gumjs
install -m 0644 -D build/frida_thin-linux-%{_arch}/include/frida-1.0/gumjs/*.h %{buildroot}/%{_includedir}/frida-1.0/gumjs
install -m 0755 -d %{buildroot}/%{_includedir}/frida-1.0/gumpp
install -m 0644 build/frida_thin-linux-%{_arch}/include/frida-1.0/gumpp/gumpp.hpp %{buildroot}/%{_includedir}/frida-1.0/gumpp/gumpp.hpp

mkdir -p %{buildroot}/%{_datadir}
install -m 0755 -d %{buildroot}/%{_datadir}/vala/vapi
install -m 0644 -D build/frida_thin-linux-%{_arch}/share/vala/vapi/frida-gum*.vapi %{buildroot}/%{_datadir}/vala/vapi


%check


%files
%{_bindir}/gum-graft
%{_libdir}/*.a
%{_libdir}/libfrida-gumpp-1.0.so
%{_libdir}/pkgconfig/*.pc
%{_includedir}/frida-1.0/gum/*.h
%{_includedir}/frida-1.0/gum/arch-arm/*.h
%{_includedir}/frida-1.0/gum/arch-arm64/*.h
%{_includedir}/frida-1.0/gum/arch-mips/*.h
%{_includedir}/frida-1.0/gum/arch-x86/*.h
%{_includedir}/frida-1.0/gum/heap/*.h
%{_includedir}/frida-1.0/gum/prof/*.h
%{_includedir}/frida-1.0/gumjs/*.h
%{_includedir}/frida-1.0/gumpp/gumpp.hpp
%{_datadir}/vala/vapi/*.vapi


%changelog

