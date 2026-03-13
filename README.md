# ngx-http-host-normalize

Nginx module to normalize the Host header when an absolute URI is used in the
request line, implementing RFC 9112 Section 3.2.2 compliance.

## The Problem

When a client sends an HTTP request with an absolute URI in the request line:

```
GET http://protected.example.com/ HTTP/1.1
Host: public.example.com
Cookie: session=xxx
```

Nginx routes the request to `protected.example.com` but passes
`HTTP_HOST=public.example.com` to backends _(FastCGI, uWSGI, SCGI, any host
configured in a `proxy_pass` whith Debian's `proxy_params` file)_.

This creates a **security vulnerability** where:

- Access control decisions based on `HTTP_HOST` use the wrong host
- Applications may serve content for the wrong virtual host

## The Solution

This module hooks into Nginx's `POST_READ` phase and normalizes the Host
header to match the host from the absolute URI, ensuring backends receive
the correct `HTTP_HOST` value.

## RFC 9112 Compliance

[RFC 9112 Section 3.2.2](https://httpwg.org/specs/rfc9112.html#absolute-form)
states:

> "When an origin server receives a request with an absolute-form of
> request-target, the origin server MUST ignore the received Host header
> field (if any) and instead use the host information of the request-target."

## Installation

### Pre-built Packages (Recommended)

Pre-built packages are available for Debian, Ubuntu, Rocky Linux, RHEL, and AlmaLinux.

**Repository:** https://linagora.github.io/ngx-http-host-normalize/

#### Debian / Ubuntu

```bash
# Add GPG key
curl -fsSL https://linagora.github.io/ngx-http-host-normalize/KEY.gpg | \
  sudo gpg --dearmor -o /etc/apt/keyrings/ngx-http-host-normalize.gpg

# Add repository (replace CODENAME with: bookworm, trixie, or sid)
echo "deb [signed-by=/etc/apt/keyrings/ngx-http-host-normalize.gpg] \
  https://linagora.github.io/ngx-http-host-normalize CODENAME main" | \
  sudo tee /etc/apt/sources.list.d/ngx-http-host-normalize.list

# Install
sudo apt update
sudo apt install libnginx-mod-http-host-normalize
```

The module is **automatically enabled** on Debian/Ubuntu.

#### Rocky Linux / RHEL / AlmaLinux

```bash
# Import GPG key
sudo rpm --import https://linagora.github.io/ngx-http-host-normalize/KEY.gpg

# Add repository
sudo tee /etc/yum.repos.d/ngx-http-host-normalize.repo << 'EOF'
[ngx-http-host-normalize]
name=Nginx Host Normalize Module
baseurl=https://linagora.github.io/ngx-http-host-normalize/rpm/el$releasever/x86_64/
enabled=1
gpgcheck=1
gpgkey=https://linagora.github.io/ngx-http-host-normalize/KEY.gpg
EOF

# Install
sudo dnf install nginx-mod-http-host-normalize
```

Then enable the module by adding this line at the **top** of `/etc/nginx/nginx.conf`:

```nginx
load_module /usr/lib64/nginx/modules/ngx_http_host_normalize_module.so;
```

### Building from Source

#### As a Dynamic Module

```bash
# Download nginx source (match your installed version)
nginx -v  # Check version
wget http://nginx.org/download/nginx-X.Y.Z.tar.gz
tar xzf nginx-X.Y.Z.tar.gz
cd nginx-X.Y.Z

# Configure with the module
./configure --with-compat --add-dynamic-module=/path/to/ngx-http-host-normalize

# Build the module
make modules

# Install the module
sudo cp objs/ngx_http_host_normalize_module.so /usr/lib/nginx/modules/
```

Then add to your `nginx.conf`:

```nginx
load_module modules/ngx_http_host_normalize_module.so;
```

#### Statically Linked

```bash
cd nginx-X.Y.Z
./configure --add-module=/path/to/ngx-http-host-normalize
make
sudo make install
```

No configuration needed - the module is automatically active.

## Configuration

Once loaded, no additional configuration is required. The module automatically
normalizes the Host header for all requests with absolute URIs.

## How It Works

1. Module registers a handler in the `NGX_HTTP_POST_READ_PHASE`
2. For each request, it checks if `r->headers_in.server` is set (indicates
   an absolute URI was used)
3. If set, it replaces the Host header value with the host from the
   request-target
4. All subsequent processing (including backend proxying) uses the
   normalized Host value

## Testing

Test the vulnerability (before installing the module):

```bash
curl -v \
  --cookie "session=xxx" \
  --header 'Host: public.example.com' \
  --request-target 'http://protected.example.com/' \
  http://protected.example.com/
```

Without the module, the backend receives `HTTP_HOST=public.example.com`.
With the module, it correctly receives `HTTP_HOST=protected.example.com`.

## Why Use This Module?

This module normalizes `$http_host` at the source, which means:

- **No configuration changes required** - existing `proxy_params`, `fastcgi_params` continue to work
- **All variables are consistent** - both `$host` and `$http_host` have the correct value
- **Third-party modules work correctly** - any module reading the Host header gets the right value
- **Logging is accurate** - access logs show the correct host

The alternatives below require configuration changes and may not cover all cases.

## Alternatives

If you cannot install this module, there are workarounds depending on your backend type.

### Understanding Nginx Variables

| Variable       | Value                                                      | Safe?                                     |
| -------------- | ---------------------------------------------------------- | ----------------------------------------- |
| `$http_host`   | Host header from client                                    | **No** - can be spoofed with absolute URI |
| `$host`        | Host from request line, then Host header, then server_name | **Yes** - prioritizes request line        |
| `$server_name` | Value from `server_name` directive                         | **Yes** - but loses alias support         |

### FastCGI / uWSGI / SCGI

The default `fastcgi_params`, `uwsgi_params`, and `scgi_params` files use `$server_name`
for SERVER_NAME, which is safe but doesn't support virtual host aliases.

**Option 1: Use SERVER_NAME (safe, but no aliases)**

Applications should use `SERVER_NAME` instead of `HTTP_HOST`:

```php
// PHP: Use $_SERVER['SERVER_NAME'] instead of $_SERVER['HTTP_HOST']
$host = $_SERVER['SERVER_NAME'];
```

**Option 2: Add HTTP_HOST with $host (safe, with aliases)**

Add to your location block, **after** the `include fastcgi_params;` line:

```nginx
location ~ \.php$ {
    include fastcgi_params;
    fastcgi_param HTTP_HOST $host;  # Must be after include
    fastcgi_pass unix:/run/php/php-fpm.sock;
}
```

This passes the correct host even with absolute URI attacks.

### Reverse Proxy (proxy_pass)

By default (without `proxy_set_header Host`), nginx sends the upstream server name
as the Host header, which is safe but breaks virtual hosting on the backend.

Debian/Ubuntu's `/etc/nginx/proxy_params` uses `$http_host`, which is **vulnerable**.

**Fix: Use $host instead of $http_host**

Either modify `/etc/nginx/proxy_params`:

```nginx
proxy_set_header Host $host;  # Changed from $http_host
proxy_set_header X-Real-IP $remote_addr;
proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
proxy_set_header X-Forwarded-Proto $scheme;
```

Or override in your server/location block:

```nginx
location / {
    proxy_pass http://backend;
    proxy_set_header Host $host;
}
```

### Testing Your Setup

You can test if your setup is vulnerable:

```bash
# Send request with mismatched Host header and request-target
curl -v \
  --header 'Host: attacker.com' \
  --request-target 'http://your-server.com/' \
  http://your-server.com/debug

# Check what HTTP_HOST your backend receives
# Vulnerable: HTTP_HOST=attacker.com
# Safe: HTTP_HOST=your-server.com
```

## Compatibility

- Nginx 1.11.5+ (for dynamic module support)
- Works with all backend protocols: FastCGI, uWSGI, SCGI, proxy_pass

## Related Issues

- [Nginx commit 71b18973b](https://github.com/nginx/nginx/commit/71b18973b) - Partial fix for FastCGI only

## License

AGPL-3 License. See [LICENSE](LICENSE) file.

## Authors and copyright

Written by [Xavier Guimard](mailto:yadd@debian.org), copyright 2026 [LINAGORA](https://linagora.com/)
