Name:           nginx-mod-http-host-normalize
Version:        0.1.0
Release:        1%{?dist}
Summary:        Nginx module to normalize Host header for absolute URI requests

License:        AGPL-3.0-or-later
URL:            https://github.com/linagora/ngx-http-host-normalize
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  nginx
BuildRequires:  nginx-mod-devel

Requires:       nginx >= 1.20

%description
An Nginx module that normalizes the Host header when an absolute URI is used
in the HTTP request line, as specified in RFC 9112 Section 3.2.2.

When a client sends a request like:
  GET http://example.com/path HTTP/1.1
  Host: other.com

This module ensures the Host header is set to "example.com" (from the
request line) rather than "other.com".

%prep
%autosetup

%build
# Get nginx configure arguments
NGINX_CONFIGURE=$(nginx -V 2>&1 | grep 'configure arguments:' | sed 's/configure arguments: //')

# Create build directory
mkdir -p build
cd build

# Download nginx source matching installed version
NGINX_VERSION=$(nginx -v 2>&1 | sed 's/.*nginx\///')
curl -sL "https://nginx.org/download/nginx-${NGINX_VERSION}.tar.gz" | tar xz
cd nginx-${NGINX_VERSION}

# Configure with compat mode for dynamic module
./configure --with-compat --add-dynamic-module=../..

# Build only the module
make modules

%install
mkdir -p %{buildroot}%{_libdir}/nginx/modules
mkdir -p %{buildroot}%{_datadir}/nginx/modules

# Install module
install -m 755 build/nginx-*/objs/ngx_http_host_normalize_module.so \
    %{buildroot}%{_libdir}/nginx/modules/

# Create module load config
cat > %{buildroot}%{_datadir}/nginx/modules/mod-http-host-normalize.conf << 'EOF'
load_module modules/ngx_http_host_normalize_module.so;
EOF

%files
%license LICENSE
%doc README.md
%{_libdir}/nginx/modules/ngx_http_host_normalize_module.so
%{_datadir}/nginx/modules/mod-http-host-normalize.conf

%post
# Enable module if not already enabled
if [ ! -f /etc/nginx/conf.d/mod-http-host-normalize.conf ]; then
    ln -sf %{_datadir}/nginx/modules/mod-http-host-normalize.conf \
        /etc/nginx/conf.d/mod-http-host-normalize.conf 2>/dev/null || true
fi

%preun
if [ "$1" = "0" ]; then
    # Remove module config on uninstall
    rm -f /etc/nginx/conf.d/mod-http-host-normalize.conf 2>/dev/null || true
fi

%changelog
* Thu Mar 13 2026 Xavier Guimard <xguimard@linagora.com> - 0.1.0-1
- Initial RPM package
