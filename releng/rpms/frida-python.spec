Name:						frida-python
Version:				%{_version}
Release:        1%{?dist}
Summary:				Python bindings for Frida

License:				wxWindows
URL:						https://github.com/frida/frida-python

BuildRequires:  python3-devel
Requires:				python3
Requires:				python3-setuptools
Requires:				python3-typing-extensions
Requires:				frida-core

%description
Python bindings for Frida


%prep


%build
make python-linux-x86_64 PYTHON=/usr/bin/python%{python3_version}


%install
install -m 0755 -d %{buildroot}/%{python3_sitearch}
install -m 0755 build/frida-linux-%{_arch}/lib/python%{python3_version}/site-packages/_frida.so %{buildroot}/%{python3_sitearch}/_frida.so
install -m 0755 -d %{buildroot}/%{python3_sitearch}/frida
install -m 0644 -D build/frida-linux-%{_arch}/lib/python%{python3_version}/site-packages/frida/*.py %{buildroot}/%{python3_sitearch}/frida


%check


%files
%{python_sitearch}/


%changelog

