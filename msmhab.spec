# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion %(uname -r)}

%{!?with_oot_debug:  %define with_oot_debug   0}

%define kmod_name msmhab

%define debug_package %{nil}

%if %{with_oot_debug}
    %define kpackage kernel-automotive-debug
    %define kversion_with_debug %{kversion}+debug
%else
    %define kpackage kernel-automotive
    %define kversion_with_debug %{kversion}
%endif

Name: %{kmod_name}
Version: 1.0
Release:        1%{?dist}
Summary:hab kernel drivers

License: GPLv2
Source0: %{name}-%{version}.tar.gz

BuildRequires: modules-signkey
BuildRequires: kernel-automotive-devel-uname-r = %{kversion_with_debug}
Requires: %{kpackage}-core-uname-r = %{kversion_with_debug}


%description
This is a rpm contains hab out of tree kernel modules.

%prep
%setup -qn %{name}

%build
KSRC=%{_usrsrc}/kernels/%{kversion_with_debug}
make KERNEL_SRC=${KSRC} modules

%post
depmod %{kversion_with_debug}

%postun
depmod %{kversion_with_debug}

%install
KSRC=%{_usrsrc}/kernels/%{kversion_with_debug}
make KERNEL_SRC=${KSRC} INSTALL_MOD_PATH=$RPM_BUILD_ROOT modules_install
make KERNEL_SRC=${KSRC} HDR_INSTAL_PATH=$RPM_BUILD_ROOT/usr/include headers_install
%{__install} -d %{buildroot}%{_sysconfdir}/modules-load.d/
%{__install} %{kmod_name}.conf %{buildroot}%{_sysconfdir}/modules-load.d/
rm -rf "$RPM_BUILD_ROOT/lib/modules/%{kversion_with_debug}/modules."*

%clean
rm -rf $RPM_BUILD_ROOT

%files
%define kernel_module_path /lib/modules/%{kversion_with_debug}
%{kernel_module_path}/extra/msm_hab.ko


%{_includedir}/linux/habmmid.h
%{_includedir}/linux/hab_ioctl.h
%{_sysconfdir}/modules-load.d/%{kmod_name}.conf
