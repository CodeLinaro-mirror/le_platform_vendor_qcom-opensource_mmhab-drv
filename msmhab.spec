# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion %(uname -r)}

%{!?with_oot_debug:  %define with_oot_debug   0}

%define kmod_name msmhab

%define debug_package %{nil}

Name: %{kmod_name}
Version: 1.0
Release:        1%{?dist}
Summary:hab kernel drivers

License: GPLv2
Source0: %{name}-%{version}.tar.gz

BuildRequires: modules-signkey
BuildRequires: kernel-automotive-devel-uname-r = %{kversion}
Requires: kernel-automotive-core-uname-r = %{kversion}

%description
This is a rpm contains hab out of tree kernel modules.

%prep
%setup -qn %{name}

%build
%if %{with_oot_debug}
KSRC=%{_usrsrc}/kernels/%{kversion}+debug
%else
KSRC=%{_usrsrc}/kernels/%{kversion}
%endif
make KERNEL_SRC=${KSRC} modules

%post
%if %{with_oot_debug}
depmod %{kversion}+debug
%else
depmod %{kversion}
%endif

%postun
%if %{with_oot_debug}
depmod %{kversion}+debug
%else
depmod %{kversion}
%endif

%install
%if %{with_oot_debug}
KSRC=%{_usrsrc}/kernels/%{kversion}+debug
%else
KSRC=%{_usrsrc}/kernels/%{kversion}
%endif
make KERNEL_SRC=${KSRC} INSTALL_MOD_PATH=$RPM_BUILD_ROOT modules_install
make KERNEL_SRC=${KSRC} HDR_INSTAL_PATH=$RPM_BUILD_ROOT/usr/include headers_install
%{__install} -d %{buildroot}%{_sysconfdir}/modules-load.d/
%{__install} %{kmod_name}.conf %{buildroot}%{_sysconfdir}/modules-load.d/
%if %{with_oot_debug}
rm -rf "$RPM_BUILD_ROOT/lib/modules/%{kversion}+debug/modules."*
%else
rm -rf "$RPM_BUILD_ROOT/lib/modules/%{kversion}/modules."*
%endif

%clean
rm -rf $RPM_BUILD_ROOT

%files
%if %{with_oot_debug}
%define kernel_module_path /lib/modules/%{kversion}+debug
%else
%define kernel_module_path /lib/modules/%{kversion}
%endif
%{kernel_module_path}/extra/msm_hab.ko


%{_includedir}/linux/habmmid.h
%{_includedir}/linux/hab_ioctl.h
%{_sysconfdir}/modules-load.d/%{kmod_name}.conf
