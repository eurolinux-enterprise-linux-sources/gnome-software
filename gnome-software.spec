%global glib2_version 2.39.1
%global gtk3_version 3.14.1
%global packagekit_version 1.0.0
%global appstream_glib_version 0.2.7
%global gsettings_desktop_schemas_version 3.11.5

Summary:   A software center for GNOME
Name:      gnome-software
Version:   3.14.7
Release:   2%{?dist}
License:   GPLv2+
Group:     Applications/System
URL:       https://wiki.gnome.org/Apps/Software
Source0:   http://download.gnome.org/sources/gnome-software/3.14/%{name}-%{version}.tar.xz

# Downstream patch to the list of unremovable system apps
Patch0:    gnome-software-system-apps.patch
# RHEL downstream patch to Editor's Picks list
Patch1:    gnome-software-editors-picks.patch
# We can't use the new name
Patch2:    0001-Change-the-name-of-the-application-to-Application-In.patch

# Update translations
Patch3:    gnome-software-3.14-ja.po.patch
Patch4:    gnome-software-3.14.7-3-more-strings-translated.patch

# backport feature for #1136954
Patch5:    0001-Show-a-new-notification-when-security-updates-remain.patch

Requires:  appstream-data
%if 0%{?fedora}
Requires:  epiphany-runtime
%endif
Requires:  glib2%{?_isa} >= %{glib2_version}
# needed for app folder .directory entries
Requires:  gnome-menus%{?_isa}
Requires:  gsettings-desktop-schemas%{?_isa} >= %{gsettings_desktop_schemas_version}
Requires:  gtk3%{?_isa} >= %{gtk3_version}
Requires:  libappstream-glib%{?_isa} >= %{appstream_glib_version}
Requires:  PackageKit%{?_isa} >= %{packagekit_version}

BuildRequires: gettext
BuildRequires: intltool
BuildRequires: libxslt
BuildRequires: docbook-style-xsl
BuildRequires: desktop-file-utils
BuildRequires: glib2-devel >= %{glib2_version}
BuildRequires: gnome-desktop3-devel
BuildRequires: gsettings-desktop-schemas-devel >= %{gsettings_desktop_schemas_version}
BuildRequires: gtk3-devel >= %{gtk3_version}
BuildRequires: libappstream-glib-devel >= %{appstream_glib_version}
BuildRequires: libsoup-devel
BuildRequires: PackageKit-glib-devel >= %{packagekit_version}
BuildRequires: polkit-devel

# this is not a library version
%define gs_plugin_version               7

%description
gnome-software is an application that makes it easy to add, remove
and update software in the GNOME desktop.

%prep
%setup -q
%patch0 -p1 -b .system-apps
%patch1 -p1 -b .editors-picks
%patch2 -p1 -b .funky-name
%patch3 -p1 -b .ja-translations
%patch4 -p1 -b .3-more-strings-translated
%patch5 -p1 -b .scary-warning

%build
%configure --disable-static
make %{?_smp_mflags}

%install
%make_install

%__rm %{buildroot}%{_libdir}/gs-plugins-%{gs_plugin_version}/*.la

# make the software center load faster
desktop-file-edit %{buildroot}%{_datadir}/applications/org.gnome.Software.desktop \
    --set-key=X-AppInstall-Package --set-value=%{name}

%find_lang %name --with-gnome

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/*.desktop

%post
touch --no-create %{_datadir}/icons/hicolor &>/dev/null || :
touch --no-create %{_datadir}/icons/HighContrast &>/dev/null || :

%postun
if [ $1 -eq 0 ]; then
    touch --no-create %{_datadir}/icons/hicolor &> /dev/null || :
    touch --no-create %{_datadir}/icons/HighContrast &> /dev/null || :
    gtk-update-icon-cache %{_datadir}/icons/hicolor &> /dev/null || :
    gtk-update-icon-cache %{_datadir}/icons/HighContrast &> /dev/null || :
    glib-compile-schemas %{_datadir}/glib-2.0/schemas &> /dev/null || :
fi

%posttrans
gtk-update-icon-cache %{_datadir}/icons/hicolor &> /dev/null || :
gtk-update-icon-cache %{_datadir}/icons/HighContrast &> /dev/null || :
glib-compile-schemas %{_datadir}/glib-2.0/schemas &> /dev/null || :

%files -f %{name}.lang
%doc AUTHORS COPYING NEWS README
%{_bindir}/gnome-software
%{_datadir}/applications/*.desktop
%dir %{_datadir}/gnome-software
%{_datadir}/gnome-software/*.png
%{_datadir}/appdata/*.appdata.xml
%{_mandir}/man1/gnome-software.1.gz
%{_datadir}/icons/hicolor/*/apps/*
%{_datadir}/icons/HighContrast/*/apps/*.png
%{_datadir}/gnome-software/featured.ini
%dir %{_libdir}/gs-plugins-%{gs_plugin_version}
%{_libdir}/gs-plugins-%{gs_plugin_version}/*.so
%{_sysconfdir}/xdg/autostart/gnome-software-service.desktop
%{_datadir}/dbus-1/services/org.gnome.Software.service
%{_datadir}/gnome-shell/search-providers/gnome-software-search-provider.ini
%{_datadir}/glib-2.0/schemas/org.gnome.software.gschema.xml
%dir %{_datadir}/gnome-software/modulesets.d
%{_datadir}/gnome-software/modulesets.d/*.xml

%changelog
* Sun Aug 30 2015 Kalev Lember <klember@redhat.com> - 3.14.7-2
- Add translations for three new strings
- Resolves: #1246388

* Mon Aug 03 2015 Kalev Lember <klember@redhat.com> - 3.14.7-1
- Update to 3.14.7
- Show installation progress when installing apps
- Fix incomplete Japanese translations
- Show a new notification when security updates remain unapplied
- Resolves: #1237181, #1055643, #1136954

* Mon Jul 13 2015 Richard Hughes <rhughes@redhat.com> - 3.14.6-5
- Do not show applications that are not available when searching by category.
- Resolves: #1237215

* Thu Jul 09 2015 Richard Hughes <rhughes@redhat.com> - 3.14.6-4
- Actually apply the last patch.
- Resolves: #1184203

* Wed Jul 08 2015 Richard Hughes <rhughes@redhat.com> - 3.14.6-3
- Use a different application name in the menu.
- Resolves: #1184203

* Wed Jul 01 2015 Kalev Lember <klember@redhat.com> - 3.14.6-2
- Update version display logic to cover the .el suffix used in RHEL
- Resolves: #1184203

* Fri Jun 19 2015 Kalev Lember <klember@redhat.com> - 3.14.6-1
- Update to 3.14.6
- Resolves: #1184203

* Wed Jun 17 2015 Kalev Lember <klember@redhat.com> - 3.14.5-2
- Add more apps to the Editor's Picks list
- Resolves: #1184203

* Tue Jun 16 2015 Richard Hughes <rhughes@redhat.com> - 3.14.5-1
- New package for RHEL
- Resolves: #1184203
