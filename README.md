# ModSim — TUBES I: Simulasi Antrian Perjalanan Bus

Simulasi discrete-event soal 2.38, *Simulation Modeling and Analysis* (Averill M. Law), menggunakan **C dan SIMLIB**.

## Isi repository

- [`TUBES1/`](TUBES1/) — source C, input/output demo, regression test, Makefile, dan sumber report.
- [`SIMLIB/`](SIMLIB/) — library dari referensi kuliah, disertakan tanpa perubahan.
- [Report PDF final](TUBES1/tubes1_report.pdf) — deskripsi masalah, source lengkap, dan output program.
- [Petunjuk lengkap](TUBES1/README.md) — format input, kebijakan model, CLI, test, dan regenerasi PDF.

## Build dan jalankan

Diperlukan GCC (C11), GNU Make, dan Bash untuk test.

```sh
make -C TUBES1
(cd TUBES1 && ./tubes1)
make -C TUBES1 test
```

CLI opsional, dengan path relatif terhadap direktori saat program dijalankan:

```sh
(cd TUBES1 && ./tubes1 tubes1.in hasil-demo.out)
```

Model berjalan selama 80 jam pada input demo, dengan rute `3 → 1 → 2 → 3`, kapasitas 20 orang, dan kecepatan 30 mil/jam. Satuan internal jam; hasil waktu ditampilkan dalam menit. Output memuat verifikasi internal, bukan klaim validasi statistik lengkap.

## Regenerasi report

Diperlukan LuaLaTeX dengan paket TikZ, listings, needspace, geometry, fancyhdr, dan microtype.

```sh
make -C TUBES1 pdf
```

Report mengambil source dan output demo secara langsung; figur rute berupa TikZ vektor. Untuk pengumpulan, gunakan PDF final saja. Sumber LaTeX dipertahankan di repository agar dapat diedit dan dibangun ulang.

Textbook lengkap, executable, dan file sementara kompilasi tidak disertakan. Tidak ada lisensi redistribusi baru yang diterapkan terhadap SIMLIB milik pihak ketiga.
