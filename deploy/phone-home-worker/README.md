# Worker gọi-về (phone-home) — trang trung tâm của Zshot

Phía NHẬN lượt gọi-về của các bản Zshot đã cài, chạy trên Cloudflare Workers + D1. Cùng hợp đồng
với worker của ZaloCRM nhưng **tách riêng** (worker riêng, D1 riêng), nên số bản cài và thông báo
của Zshot không trộn với ZaloCRM.

## Phân biệt nhiều phần mềm trên `updater.zopen.vn`

Mỗi phần mềm gọi về một đường dẫn riêng; body vẫn chỉ `{ instanceId, version }`:

| Phần mềm | Gọi về | Worker |
|---|---|---|
| ZaloCRM / zcrm-core | `updater.zopen.vn/v1/ping` | `zalocrm-phone-home` (route `updater.zopen.vn/*`) |
| Zshot | `updater.zopen.vn/zshot/v1/ping` | `zshot-phone-home` (route `updater.zopen.vn/zshot/*`) |

Cloudflare chọn route **cụ thể nhất**, nên `/zshot/*` về worker này, mọi đường dẫn khác vẫn về
worker ZaloCRM như cũ.

## Nó làm gì

Bản cài (`src/core/phonehome.cpp`, tiến trình ở khay hệ thống) gọi `POST /zshot/v1/ping` 30 giây
sau khi khởi động và mỗi 12 giờ. Worker:

1. Ghi/cập nhật một dòng trong bảng `instances` theo `instance_id` (lần đầu thấy, lần cuối thấy,
   bản đang chạy). **Không ghi IP, không ghi header.**
2. Trả về thông báo `enabled` mới nhất khớp điều kiện (`min_version` / `max_version` /
   `target_instance_id`), hoặc `{ announcement: null }` ⇒ dải trong Zshot ẩn.

## Dựng lần đầu

```bash
cd deploy/phone-home-worker
npx wrangler login
npx wrangler d1 create zshot-phone-home        # chép database_id vào wrangler.toml
npx wrangler d1 execute zshot-phone-home --remote --file=schema.sql
npx wrangler secret put ADMIN_TOKEN            # chuỗi dài ngẫu nhiên: openssl rand -hex 32
npx wrangler deploy
```

Zone `zopen.vn` phải nằm trên Cloudflare và bản ghi `updater` đã proxied (đã có sẵn cho ZaloCRM).

## Dùng hằng ngày

```bash
T="Bearer $ADMIN_TOKEN"; B=https://updater.zopen.vn/zshot/v1/admin

# Thống kê: đã cài / đang dùng 7 ngày / 30 ngày / theo bản
curl -s -H "Authorization: $T" $B/stats

# Thông báo "có bản 1.1" CHỈ cho bản cũ hơn 1.1
curl -s -X PUT -H "Authorization: $T" -H 'content-type: application/json' $B/announcements/rel-1.1 \
  -d '{"text":"Đã có Zshot 1.1 — thêm thanh công cụ mới.","level":"info",
       "link":"https://github.com/locphamnguyen/Zshot/releases/latest","linkLabel":"Cách cập nhật",
       "maxVersion":"1.0.99"}'

# Tắt dải (mọi bản cài ẩn trong vòng tối đa 12 giờ)
curl -s -X PUT -H "Authorization: $T" -H 'content-type: application/json' $B/announcements/rel-1.1 \
  -d '{"text":"(tắt)","enabled":false}'

# Xoá hẳn
curl -s -X DELETE -H "Authorization: $T" $B/announcements/rel-1.1
```

Trường của một thông báo: `text` (thuần, ≤ 500 ký tự), `level` (`info` / `warning` /
`critical`), `link` (chỉ https), `linkLabel`, `dismissible` (false = không có nút đóng, cho cập
nhật bảo mật), `enabled`, `minVersion` / `maxVersion`, `targetInstanceId`. **Đổi `id` mới** khi
muốn người đã bấm đóng thấy lại.
