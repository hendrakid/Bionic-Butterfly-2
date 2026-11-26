# Link Monitor Diagnostics

Link Monitor adalah aplikasi desktop lokal di [`tools/link_monitor`](../tools/link_monitor/) untuk membaca log USB serial Controller dan Receiver bersamaan. Aplikasi tidak memakai API radio, tidak menjadi peer ESP-NOW, tidak mengirim serial payload, dan tidak mengubah protokol atau firmware.

## Model observasi

Firmware mencetak counter kumulatif sekitar sekali per detik. Karena waktu log kedua board tidak tersinkron dan satu snapshot dapat berada pada sisi berbeda dari suatu paket, diagram merupakan **alur agregat**, bukan trace per sequence/paket.

| Tahap UI | Log/field | Yang dapat disimpulkan |
|---|---|---|
| Controller — Control submitted | `CTRL controls` | Transport menerima permintaan kirim control |
| Receiver — Transport frames | `RX rx_frames` | Transport receiver mengantre frame nonempty yang size-acceptable sebelum validasi aplikasi; belum pasti control yang valid |
| Receiver — Validated controls | `RX controls` | Control lolos validasi aplikasi |
| Receiver — ACK submitted | `RX acks` | Transport menerima permintaan kirim ACK; belum membuktikan ACK diterima Controller |
| Controller — ACK confirmed | `CTRL confirmed_acks` | Controller menerima dan memvalidasi ACK yang masih dalam deadline |

`radio_ok` hanya MAC-layer send callback dan bukan application delivery. `RX expired` adalah frame antrean yang ditolak karena sudah berumur sekurangnya receiver link timeout. `future` adalah subset `expired` dan tidak dijumlahkan lagi. `rejected` menggabungkan beberapa kemungkinan penolakan; log saat ini tidak memberi alasan per paket.

## Persentase ACK dan gap

Dalam satu segmen counter Controller:

$$
\text{ACK coverage} = 100\% \times \frac{\Delta\,\texttt{confirmed\_acks}}{\Delta\,\texttt{controls}}
$$

- Denominator nol, baseline belum lengkap, malformed log, atau discontinuity menghasilkan `N/A`.
- `unconfirmed gap = Δcontrols − Δconfirmed_acks` hanya jika hasil nonnegatif. Ini jumlah control yang **belum/tidak terkonfirmasi dalam batas capture**, bukan exact lost atau timeout count.
- Firmware tidak mencetak counter pending expiry. Gap dapat meliputi ACK yang masih outstanding pada batas capture, ACK terlambat, ACK hilang, control hilang, atau perbedaan snapshot. Monitor tidak mengklaim penyebab.
- `ack_rtt_ms` adalah sampel RTT ACK terakhir, bukan mean, percentile, atau distribusi.

Counter dihitung sebagai delta setelah baseline pertama yang baru setelah Start. Counter turun, reboot, boot banner, atau perubahan waktu yang ambigu memulai segmen dan baseline baru. Rollover uint32 hanya diterima di dekat batas dengan delta yang masuk akal. Setelah discontinuity satu board, perbandingan lintas board ditandai tidak andal.

## Lifecycle dan kualitas capture

- Connect membuka dua serial 115200 secara read-only pada tingkat aplikasi. DTR/RTS dibuat inactive sebelum open sejauh didukung pyserial/driver; driver USB tetap dapat menghasilkan reset/transient, sehingga perilaku harus diuji pada hardware aktual.
- Start memerlukan role `CTRL`/`RX` yang terverifikasi dan stats baru dari kedua board. Stop menyimpan hasil, tetapi tidak menutup port atau menghentikan radio/firmware.
- USB disconnect, stats serial stale, overflow buffer, disk error, malformed line, dan role mismatch ditampilkan sebagai warning/incomplete. Stats serial stale tidak sama dengan radio link loss.
- JSONL menyimpan metadata, raw line yang diparse, lifecycle/summary, wall clock PC, elapsed monotonic PC, dan field board. CSV adalah turunan snapshot metrik, bukan packet list.
- Replay memakai parser/metric engine yang sama dan tidak membuka hardware. File terpotong dapat direplay hingga baris JSON lengkap terakhir dengan warning; schema yang tidak didukung ditolak.

Data dari dua board tidak mempunyai common clock. Jangan menggunakan waktu kedatangan serial sebagai waktu paket ESP-NOW atau menghitung latency cross-board darinya.

## Validasi hardware wajib

Compilation, unit test, GUI smoke test, dan replay tidak membuktikan bahwa penggunaan USB tidak memengaruhi radio. Sebelum hasil dipakai sebagai baseline:

1. Tutup semua serial monitor lain; pertahankan power, posisi, jarak, orientasi, build, dan board yang sama.
2. Pastikan receiver tetap DISARMED dan servo disabled; siapkan mitigasi mekanis terpisah.
3. Catat counter/link sebelum Connect. Connect kedua port dan periksa apakah salah satu board reboot, disconnect, atau mengalami perubahan timing.
4. Bandingkan delta control/ACK/RTT/overflow sebelum monitor, selama capture, dan setelah Stop. Stop harus tidak menghentikan komunikasi firmware.
5. Ulangi Start/Stop tanpa membuka ulang port, USB disconnect/reconnect, reboot satu board, serta link loss yang dikendalikan pengguna.
6. Dokumentasikan driver USB, port, build ID, MAC aktual, durasi, lokasi/orientasi, dan semua warning capture.

Tidak ada threshold performa baru sampai baseline aktual tersedia. Edge case expired/malformed dapat diuji secara aman melalui fixture/replay; jangan menyuntik radio untuk memaksa kondisi hanya demi dashboard.
