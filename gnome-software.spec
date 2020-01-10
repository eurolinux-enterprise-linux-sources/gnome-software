%global glib2_version 2.46.0
%global gtk3_version 3.22.4
%global json_glib_version 1.2.0
%global packagekit_version 1.1.1
%global appstream_glib_version 0.7.8
%global libsoup_version 2.52.0
%global gsettings_desktop_schemas_version 3.12.0
%global gnome_desktop_version 3.18.0
%global fwupd_version 1.0.7
%global flatpak_version 0.9.4

# this should be set using "--without packagekit" when atomic
%bcond_without packagekit

Name:      gnome-software
Version:   3.28.2
Release:   3%{?dist}
Summary:   A software center for GNOME

License:   GPLv2+
URL:       https://wiki.gnome.org/Apps/Software
Source0:   https://download.gnome.org/sources/gnome-software/3.28/%{name}-%{version}.tar.xz

# We can't use the new name
Patch2:    0001-Change-the-name-of-the-application-to-Application-In.patch
Patch3:    downstream-translations.patch

BuildRequires: gettext
BuildRequires: intltool
BuildRequires: libxslt
BuildRequires: docbook-style-xsl
BuildRequires: desktop-file-utils
BuildRequires: fwupd-devel >= %{fwupd_version}
BuildRequires: glib2-devel >= %{glib2_version}
BuildRequires: gnome-desktop3-devel
BuildRequires: gsettings-desktop-schemas-devel >= %{gsettings_desktop_schemas_version}
BuildRequires: gspell-devel
BuildRequires: gtk3-devel >= %{gtk3_version}
BuildRequires: gtk-doc
BuildRequires: json-glib-devel >= %{json_glib_version}
BuildRequires: libappstream-glib-devel >= %{appstream_glib_version}
BuildRequires: libsoup-devel
BuildRequires: meson
BuildRequires: PackageKit-glib-devel >= %{packagekit_version}
BuildRequires: polkit-devel
BuildRequires: libsecret-devel
BuildRequires: flatpak-devel >= %{flatpak_version}
BuildRequires: libgudev1-devel
BuildRequires: liboauth-devel

Requires: appstream-data
%if 0%{?fedora}
Requires: epiphany-runtime
%endif
Requires: flatpak%{?_isa} >= %{flatpak_version}
Requires: flatpak-libs%{?_isa} >= %{flatpak_version}
Requires: fwupd%{?_isa} >= %{fwupd_version}
Requires: glib2%{?_isa} >= %{glib2_version}
Requires: gnome-desktop3%{?_isa} >= %{gnome_desktop_version}
# gnome-menus is needed for app folder .directory entries
Requires: gnome-menus%{?_isa}
Requires: gsettings-desktop-schemas%{?_isa} >= %{gsettings_desktop_schemas_version}
Requires: gtk3%{?_isa} >= %{gtk3_version}
Requires: json-glib%{?_isa} >= %{json_glib_version}
Requires: iso-codes
Requires: libappstream-glib%{?_isa} >= %{appstream_glib_version}
# librsvg2 is needed for gdk-pixbuf svg loader
Requires: librsvg2%{?_isa}
Requires: libsoup%{?_isa} >= %{libsoup_version}
Requires: PackageKit%{?_isa} >= %{packagekit_version}

# Removed in RHEL 7.6
Obsoletes: gnome-shell-browser-plugin < 3.28.3-4

# this is not a library version
%define gs_plugin_version               11

%description
gnome-software is an application that makes it easy to add, remove
and update software in the GNOME desktop.

%package devel
Summary: Headers for building external gnome-software plugins
Requires: %{name}%{?_isa} = %{version}-%{release}

%description devel
These development files are for building gnome-software plugins outside
the source tree. Most users do not need this subpackage installed.

%package editor
Summary: Banner designer for GNOME Software
Requires: %{name}%{?_isa} = %{version}-%{release}

%description editor
Editor for designing banners for GNOME Software.

%prep
%autosetup -p1

%build
%meson \
    -Denable-valgrind=false \
    -Denable-snap=false \
    -Denable-fwupd=true \
    -Denable-gudev=true \
%if %{with packagekit}
    -Denable-packagekit=true \
%else
    -Denable-packagekit=false \
%endif
    -Denable-external-appstream=false \
    -Denable-rpm-ostree=false \
    -Denable-tests=false \
    -Denable-ubuntuone=false \
    -Denable-ubuntu-reviews=false
%meson_build

%install
%meson_install

# make the software center load faster
desktop-file-edit %{buildroot}%{_datadir}/applications/org.gnome.Software.desktop \
    --set-key=X-AppInstall-Package --set-value=%{name}

# set up for Fedora
cat >> %{buildroot}%{_datadir}/glib-2.0/schemas/org.gnome.software-fedora.gschema.override << FOE
[org.gnome.software]
official-repos = [ 'rhel-7' ]
FOE

%find_lang %name --with-gnome

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/*.desktop

%post
touch --no-create %{_datadir}/icons/hicolor &>/dev/null || :

%postun
if [ $1 -eq 0 ]; then
    touch --no-create %{_datadir}/icons/hicolor &> /dev/null || :
    gtk-update-icon-cache %{_datadir}/icons/hicolor &> /dev/null || :
    glib-compile-schemas %{_datadir}/glib-2.0/schemas &> /dev/null || :
fi

%posttrans
gtk-update-icon-cache %{_datadir}/icons/hicolor &> /dev/null || :
glib-compile-schemas %{_datadir}/glib-2.0/schemas &> /dev/null || :

%files -f %{name}.lang
%doc AUTHORS README.md
%license COPYING
%{_bindir}/gnome-software
%{_datadir}/applications/gnome-software-local-file.desktop
%{_datadir}/applications/org.gnome.Software.desktop
%dir %{_datadir}/gnome-software
%{_datadir}/gnome-software/*.png
%{_datadir}/doc/gnome-software/README.md
%{_mandir}/man1/gnome-software.1.gz
%{_datadir}/icons/hicolor/*/apps/*
%{_datadir}/gnome-software/featured-*.svg
%{_datadir}/gnome-software/featured-*.jpg
%{_datadir}/metainfo/org.gnome.Software.appdata.xml
%{_datadir}/metainfo/org.gnome.Software.Plugin.Epiphany.metainfo.xml
%{_datadir}/metainfo/org.gnome.Software.Plugin.Flatpak.metainfo.xml
%{_datadir}/metainfo/org.gnome.Software.Plugin.Fwupd.metainfo.xml
%{_datadir}/metainfo/org.gnome.Software.Plugin.Odrs.metainfo.xml
%{_datadir}/metainfo/org.gnome.Software.Plugin.Steam.metainfo.xml
%dir %{_libdir}/gs-plugins-%{gs_plugin_version}
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_appstream.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_desktop-categories.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_desktop-menu-path.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_dpkg.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_dummy.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_epiphany.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_fedora-pkgdb-collections.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_flatpak.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_fwupd.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_generic-updates.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_hardcoded-blacklist.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_hardcoded-featured.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_hardcoded-popular.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_icons.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_key-colors-metadata.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_key-colors.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_modalias.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_odrs.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_os-release.so
%if %{with packagekit}
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_packagekit-history.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_packagekit-local.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_packagekit-offline.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_packagekit-proxy.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_packagekit-refine-repos.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_packagekit-refine.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_packagekit-refresh.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_packagekit-upgrade.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_packagekit-url-to-app.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_packagekit.so
%endif
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_provenance-license.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_provenance.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_repos.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_rewrite-resource.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_shell-extensions.so
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_steam.so
%if %{with packagekit}
%{_libdir}/gs-plugins-%{gs_plugin_version}/libgs_plugin_systemd-updates.so
%endif
%{_sysconfdir}/xdg/autostart/gnome-software-service.desktop
%{_datadir}/app-info/xmls/org.gnome.Software.Featured.xml
%if %{with packagekit}
%{_datadir}/dbus-1/services/org.freedesktop.PackageKit.service
%endif
%{_datadir}/dbus-1/services/org.gnome.Software.service
%{_datadir}/gnome-shell/search-providers/org.gnome.Software-search-provider.ini
%{_datadir}/glib-2.0/schemas/org.gnome.software.gschema.xml
%{_datadir}/glib-2.0/schemas/org.gnome.software-fedora.gschema.override
%{_libexecdir}/gnome-software-cmd
%{_libexecdir}/gnome-software-restarter

%files devel
%{_libdir}/pkgconfig/gnome-software.pc
%dir %{_includedir}/gnome-software
%{_includedir}/gnome-software/*.h
%{_datadir}/gtk-doc/html/gnome-software

%files editor
%{_bindir}/gnome-software-editor
%{_datadir}/applications/org.gnome.Software.Editor.desktop
%{_mandir}/man1/gnome-software-editor.1*

%changelog
* Mon Sep 10 2018 Kalev Lember <klember@redhat.com> - 3.28.2-3
- Obsolete gnome-shell-browser-plugin
- Resolves: #1626104

* Thu Jul 12 2018 Richard Hughes <rhughes@redhat.com> - 3.28.2-2
- Set the repo provenance properly, showing the source line where required.
- Resolves: #1592809

* Fri Jun 08 2018 Richard Hughes <rhughes@redhat.com> - 3.28.2-1
- Update to 3.28.2
- Resolves: #1568625

* Wed Feb 07 2018 Kalev Lember <klember@redhat.com> - 3.22.7-5
- Backport a fix for installing flatpakref files
- Resolves: #1509883

* Tue Jan 23 2018 Ray Strode <rstrode@redhat.com> - 3.22.7-4
- Drop non-free sources from schema to avoid confusing infobar at startup
  Resolves: #1514113

* Fri Nov 03 2017 Richard Hughes <rhughes@redhat.com> - 3.22.7-3
- Never show 'Startup Applications' in the search results
- Note: the other issue shown in the bug is likely a dupe of #1445651
- Resolves: #1481145

* Tue Sep 19 2017 Richard Hughes <rhughes@redhat.com> - 3.22.7-2
- Backport two fixes to fix a common error message
- Resolves: #1445651

* Tue Mar 14 2017 Kalev Lember <klember@redhat.com> - 3.22.7-1
- Update to 3.22.7
- Resolves: #1386961

* Wed Mar 08 2017 Kalev Lember <klember@redhat.com> - 3.22.6-1
- Update to 3.22.6
- Build with flatpak support
- Resolves: #1386961

* Thu Mar 02 2017 Richard Hughes <rhughes@redhat.com> - 3.22.5-1
- Update to 3.22.5
- Resolves: #1386961

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
