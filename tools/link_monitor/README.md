# Link Monitor

Aplikasi Windows lokal untuk **membaca log USB serial saja** dari Controller dan Receiver secara bersamaan. Tidak memakai Wi-Fi atau ESP-NOW, tidak mengirim data ke board, dan tidak mengubah firmware. Halaman [panduan diagnostik](../../docs/LINK_MONITOR.md) menjelaskan arti metrik dan batas pengamatan.

## Persyaratan

- Windows, Python 3.12, dua kabel/port USB serial; kedua firmware memakai baud 115200.
- Port serial tidak bisa dibuka bersamaan oleh PlatformIO Serial Monitor, aplikasi lain, dan Link Monitor. Tutup serial monitor lain dahulu.
- Port aktual mungkin bukan COM4/COM10 setelah reconnect. Memilih port hanya mengidentifikasi sumber serial; aplikasi memeriksa bahwa log yang masuk memang `CTRL` atau `RX`.

## Instalasi dan menjalankan

Dari folder `tools\link_monitor` di PowerShell:

```powershell
py -3.12 -m venv .venv
.\.venv\Scripts\python.exe -m pip install -e ".[test,package]"
.\.venv\Scripts\python.exe -m link_monitor
```

Tidak ada koneksi cloud. Dependensi tercantum dan dikunci dalam `pyproject.toml`. Data disimpan di folder yang pengguna pilih, default `Documents\BionicButterfly\LinkMonitor`. Folder `.venv`, build, dist, cache Python, dan log build tidak perlu dikomit.

### Membuka executable Windows

Executable tidak disimpan di Git; folder `dist` di-ignore. Setelah menjalankan langkah packaging di bawah dari folder `tools\link_monitor`, buka aplikasi dari root repository dengan:

```powershell
.\tools\link_monitor\dist\LinkMonitor\LinkMonitor.exe
```

Pastikan seluruh folder `dist\LinkMonitor` hasil build tersedia. Jika belum ada, gunakan langkah instalasi di atas (`python -m link_monitor`) atau build executable terlebih dahulu.

## Cara pakai

1. Nyalakan kedua board dan pastikan receiver tetap dalam keadaan aman/DISARMED. Jangan gunakan aplikasi sebagai pengganti prosedur keselamatan mekanis.
2. Refresh daftar port, pilih port Controller dan Receiver yang **berbeda**, lalu tekan **Connect** per perangkat. Tunggu `role verified`. Membuka USB serial dapat mereset beberapa board karena driver/DTR/RTS. Aplikasi menonaktifkan DTR/RTS sebelum membuka port sebatas kemampuan driver; ini **bukan jaminan** bebas reset.
3. Tekan **Start**. Pengukuran menunggu baseline berupa stats pertama **setelah Start** pada tiap board. Data sebelum baseline tidak dihitung. Selama berjalan, status/diagram diperbarui mengikuti log berkala (~1 detik). Tidak ada animasi paket yang dikarang.
4. Tekan **Stop** untuk membekukan hasil dan menutup file sesi; kedua port tetap terbuka dan firmware terus berkomunikasi. Start lagi membuat sesi baru.
5. **Export CSV** menghasilkan tabel snapshot/delta metrik. **Open replay JSONL** membaca sesi dari disk: hentikan capture, putuskan kedua port secara eksplisit, lalu Play/Pause, Restart, atau Exit replay. Replay tidak mengakses radio atau serial.
6. Tekan **Disconnect** untuk melepaskan port. Jangan membuka serial monitor lain sebelum ini.

Jika USB terputus, role salah, log berhenti, buffer overflow, atau penyimpanan gagal, hasil diberi label `incomplete`/quality warning. Stop tidak menyembunyikan kegagalan sebelumnya. File JSONL bersifat append-only dan dapat direplay sampai record terakhir lengkap bila penulisan terputus; cek warning incomplete.

## Pengujian dan packaging

```powershell
.\.venv\Scripts\python.exe -m pytest -q
.\.venv\Scripts\python.exe -m link_monitor --smoke-test
.\.venv\Scripts\python.exe -m PyInstaller --noconfirm --clean link_monitor.spec
.\dist\LinkMonitor\LinkMonitor.exe --smoke-test --smoke-output smoke.json
```

Distribusi one-folder berada di `dist\LinkMonitor`; seluruh folder harus disalin bersama exe. `--smoke-test` membuka UI singkat tanpa port/radio dan keluar, **bukan uji hardware**. Hapus `smoke.json` setelah pemeriksaan. Firmware tidak dibangun ulang karena source/PlatformIO-nya tidak diubah.

### Troubleshooting

- Port sibuk: tutup Serial Monitor PlatformIO dan pilih port aktual.
- Role belum terverifikasi: tunggu stats valid atau periksa pemetaan kabel/baud; Start mensyaratkan stats kedua board yang baru.
- Log `stale` berarti tidak ada stats serial baru; **tidak otomatis berarti radio mati**.
- Setelah reboot board, monitor membuat segmen/baseline baru, bukan menjumlahkan counter reboot dengan counter sesi sebelumnya.
- Capturing dan replay tidak boleh dicampur. File sesi tidak ditulis di source repository secara default.
