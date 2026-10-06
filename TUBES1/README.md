# TUBES I — Simulasi Antrian Perjalanan Bus

Implementasi soal 2.38 menggunakan C dan SIMLIB.

## Berkas utama

- `tubes1.c` — source code simulasi.
- `tubes1.in` — input demo: 80 jam, laju 14/10/24 orang per jam, kapasitas 20, kecepatan 30 mil/jam.
- `tubes1.out` — output dari source code dan input demo.
- `tubes1_report.pdf` — report yang dikumpulkan. Report memuat deskripsi masalah, source code, dan output.
- `Makefile` — perintah build, run, dan regenerasi report.

Berkas kerja untuk regenerasi PDF:

- `tubes1_report.tex` — source LaTeX; tidak perlu dikumpulkan jika yang diminta hanya PDF.
- `car_rental_figure.tex` — gambar rute vektor TikZ yang dipakai oleh source LaTeX.

Folder ini diharapkan berada di dalam folder `ModSim` sehingga SIMLIB tersedia pada `../SIMLIB/`.

## Build dan run simulasi

```sh
make clean
make
./tubes1
```

Tanpa argumen, executable membaca `tubes1.in` dan menulis `tubes1.out` dari current working directory.
Untuk skenario lain: `./tubes1 path/input.in path/output.out`. Satu argumen mengganti input saja.
Direktori output harus sudah ada; input/output yang menunjuk file sama (termasuk link) ditolak.
Format input tetap memakai jam, orang/jam, kapasitas orang, dan mil/jam.
Simulasi internal tetap dalam jam; delay, stop, loop, dan waktu sistem pada output ditampilkan dalam menit.
Output memuat cek kapasitas, konservasi termasuk orang yang sedang boarding, invariant tiap event,
minimum stop/loop, dan horizon. Batas minimum loop dihitung dari parameter kecepatan dan jarak.
Tanpa observasi stop/loop, cek diberi `SKIP`. Kegagalan cek menghasilkan exit code nonzero.
Cek internal bukan bukti validasi statistik atau kesesuaian semua interpretasi soal.

Input divalidasi sebelum output dibuka. Durasi, laju kedatangan, dan kecepatan harus finite serta lebih besar dari nol; kapasitas bus harus bilangan bulat positif.

## Regenerasi PDF

Perintah berikut menjalankan simulasi demo kemudian mengompilasi report dua kali:

```sh
make pdf
```

Target `pdf` memakai LuaLaTeX dan memerlukan `tubes1_report.tex` serta `car_rental_figure.tex`. Untuk penyerahan, file yang diperlukan adalah `tubes1_report.pdf`.

## Regression test

`make test` menjalankan `test_input.sh` (format/domain input dan preservasi output)
serta `test_features.sh` (CLI, konversi tampilan, verifikasi, alias file, horizon pendek,
penumpang sedang boarding, dan batas loop yang mengikuti parameter).
