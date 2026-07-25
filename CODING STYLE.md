# CODING STYLE

Per tanggal 25 Juli 2026, VeaOS resmi menetapkan penentuan cara untuk mengoding project dengan cara yang benar. Tujuan diberadakannya CODING STYLE ini untuk:

1. Menciptakan kode yang source-nya mudah dibaca.
2. Memberikan informasi seputar file.
3. Agar kode tidak mudah berserakan di Repository tanpa alasan yang jelas.

Terima kasih untuk Linux dan ReactOS atas inspirasinya untuk menetapkan CODING STYLE.

---

## 1. Penulisan Indentasi
Indentasi wajib digunakan untuk memperjelas hierarki dan keterbacaan *source code*.

*   **Lebar Identasi:** Gunakan 4 spasi untuk setiap tingkat (level) indentasi. (Dilarang mencampur Tab dan Spasi secara acak).

    **Benar:**
    ```c
    VOID
    VEAPI
    Foo(VOID)
    {
        DoSomething();
    }
    ```

    **Salah:**
    ```c
    VOID 
    VEAPI
    Foo(VOID)
    {
    DoSomething();
       AtauIndentasiSepertiIniSalah();
                   TidakBolehSejauhIniDariTingkatan();
    }
    ```

*   **Brace Style:** Menggunakan *Allman Style*. Setiap *braces* `{` dan `}` wajib memiliki barisnya tersendiri.

    **Benar:**
    ```c
    VOID
    VEAPI
    Foo(VOID)
    {
        DoSomething();
        if (Some)
        {
            
        }
    }
    ```

    **Salah:**
    ```c
    VOID
    VEAPI
    Foo(VOID) {
        // linux style sangat DILARANG
        if (SOME){
         // ^^ apalagi braces tertempel dengan kurung kurawal
        }
    }
    ```

*   **Maksimal Level Kedalaman:** VeaOS mewajibkan maksimal 4 kedalaman level indentasi untuk setiap fungsi kode yang ada. Jika kode Anda ternyata memerlukan lebih dari 4 kedalaman level, segera pecah kode itu menjadi *helper*, atau kecilkan fungsi Anda, atau perbaiki logika kodenya.

---

## 2. Cara Penulisan
Penulisan yang benar membuat kode menjadi jauh lebih nyaman untuk dibaca.

*   **PascalCase:** Setiap kode mulai dari *Variable* dan *Function* (`typedef variable` wajib menggunakan `ALL_UPPERCASE`) wajib menggunakan `PascalCase` supaya kode lebih nyaman dibaca dan yang pasti tidak membuat keyboard Anda cepat panas :p.
    *Style yang wajib dihindari:*
    1. `camelCase`
    2. `snake_case`

    **Benar:**
    ```c
    VOID
    VEAPI
    AkuCintaHatsuneMiku(
        IN INT Aku,
        OUT INT Miku
    )
    {
        IniBaruBener(Aku, Miku);
    }
    ```

    **Salah:**
    ```c
    void veapi aku_cinta_hatsune_miku(int aku, int miku)
    {
        // Walau identasi sudah benar dan braces sesuai, ini masih dilarang
        ini_salah(aku, miku)
    }
    ```

*   **Pendefinisian Variable:** Definisi *Variable* wajib memiliki lebih dari 2 huruf. Dan *Variable* tersebut harus memiliki nama yang jelas sesuai dengan tujuan awal kenapa *Variable* tersebut dibuat. Terlebih juga, harus mengikuti `PascalCase`.

    **Benar:**
    ```c
    BOOLEAN DidSend;
    INT TotalSend;
    ```

    **Salah:**
    ```c
    BOOLEAN A // <- DILARANG
    BOOLEAN bA // <- TETAP DILARANG WALAU 2 HURUF TAPI camelCase
    ```

*   **Pendefinisian Fungsi:** Pendefinisian Fungsi juga memiliki aturan tertulisnya.
    
    *Tanpa Argumen (`VOID`):*
    ```c
    RETURN_TYPE
    CALLING_CONVENTION
    NamaFungsi(VOID); // <- WAJIB TARO VOID JIKA TIDAK MENERIMA ARGUMEN/VARIABLE
    ```
    
    *Dengan 1 Argumen:*
    ```c
    RETURN_TYPE
    CALLING_CONVENTION
    NamaFungsi(IN INT Awalan); // <- 1 Fungsi tetap harus ditaro di line yang sama dengan nama fungsi
    ```
    
    *Dengan 2 atau lebih argumen:*
    ```c
    RETURN_TYPE
    CALLING_CONVENTION
    NamaFungsi(
        IN INT Budi, // <- Ini juga kena newline jika 2 variable
        OUT INT Santoso // <- Wajib newline setiap variable
    );
    ```

*   **Macro Preprocessor:** Macro Preprocessor seperti `#define` wajib memiliki aturan penamaan sebagai berikut:
    1. DILARANG menggunakan `PascalCase`. Wajib `ALL_UPPER_CASE`.
    2. DILARANG menempatkan Macro Preprocessor di dalam fungsi (WAJIB DI LUAR FUNGSI).
    3. GUNAKAN penamaan yang jelas dan bermakna ketika membuat Macro Preprocessor.
    4. Jika membuat 2 Macro yang saling berdampingan dengan line sebelahnya, samakan indentasi MACRO EXPAND-nya dengan MACRO EXPAND di sebelahnya.

    **Contoh:**
    ```c
    #define APIC_EOI                0xB0
    #define APIC_REG_TPR            0x080
    #define APIC_BASE_ADDRESS       0xFEE00000
    #define IOAPIC_BASE_ADDRESS     0xFEC00000
    #define IOAPIC_REGSEL           0x00
    #define IOAPIC_IOWIN            0x10
    #define APIC_REG_ICR_LOW        0x300
    #define APIC_REG_ICR_HIGH       0x310
    #define APIC_REG_EOI            0x0B0
    ```

*   **Komentar:** Komentar adalah dokumentasi mini yang ditempatkan di dalam kode agar orang yang ingin membacanya tidak kebingungan dengan apa maksud kode itu.
    
    *Aturan dasar:*
    1. Gunakan `//` untuk single line atau multi line.
    2. Atau bisa menggunakan `/* KOMENTAR */` juga.
    3. DILARANG mengkomentari dead-code. Jika ada dead-code, HAPUS! Jangan dibiarkan disana.
    4. Setiap blok kode yang memiliki maksud di dalam sebuah fungsi, WAJIB diberikan komentar agar orang lain tahu maksudnya apa. Pastikan komentarnya itu jelas dan tidak menimbulkan banyak pertanyaan.
    5. Dilarang mengulang redundansi komen yang padahal sudah jelas.

    **Benar:**
    ```c
    VOID
    VEAPI
    ApicEnableLapic(VOID)
    {
        // Check apakah Base Address LAPIC sudah di-map ke Virtual Memory
        if (ApicLapicBase == NULL)
        {
            KdPrintf("HCT: LAPIC Base Address belum di-map!\n\r");
            return;
        }

        /* 
         * Konfigurasi Spurious Interrupt Vector Register (SIVR):
         * - Bit 8   : APIC Software Enable
         * - Bit 0-7 : Spurious Vector (0xFF / 255)
         */
        ULONG Sivr = ApicLapicBase[LAPIC_SIVR_OFFSET / 4];
        Sivr |= 0x100;
        Sivr |= 0x0FF;
        ApicLapicBase[LAPIC_SIVR_OFFSET / 4] = Sivr;

        // Reset TPR ke 0 agar hardware menerima semua Interrupt Level (IRQL PASSIVE_LEVEL)
        ApicLapicBase[LAPIC_TPR_OFFSET / 4] = 0;
    }
    ```

    **Salah:**
    ```c
    VOID
    VEAPI
    ApicEnableLapic(VOID)
    {
        if (ApicLapicBase == NULL)
        {
            return; // Tidak ada penjelasan kenapa fungsi langsung return
        }

        // ApicLapicBase[0] = 0; <-- DILARANG: Dead code dibiarkan ter-comment! HAPUS!

        ULONG Sivr = ApicLapicBase[LAPIC_SIVR_OFFSET / 4]; // Ambil Sivr <-- DILARANG: Komentar redundan/mengulang apa yang terlihat di kode

        Sivr |= 0x100; // Set bit ke 100 hex <-- Komentar tidak menjelaskan TUJUAN (kenapa di-set 0x100?)
        ApicLapicBase[LAPIC_SIVR_OFFSET / 4] = Sivr;

        // OldApicDisableLogic(); <-- DILARANG: Jangan tinggalkan sisa uji coba kode lama
    }
    ```

*   **Pointer Style:** Pointer style yang benar adalah ditempel di sebelah kanan *Variable Name*.

    **Benar:**
    ```c
    VOID *Ajl;
    PVOID *AhciInterruptRoutine;
    ```

    **Salah:**
    ```c
    VOID* AduhSalah; // <- SALAH
    PVOID * HarusnyaSalah; // <- SALAH JUGA
    ```

*   **Penamaan Struct dan Enumeration:** Nama struct wajib di-`typedef` ke nama `UPPER_CASE` dan struct wajib diberikan namanya dengan awalan `_` di awal sebagai tanda agar tidak bentrok, dan enum juga sama. Setiap variable yang ada di dalam struct wajib diberikan variable `PascalCase`, sama dengan aturan `PascalCase` yang dibilang di atas.

    **Benar:**
    ```c
    typedef struct _VEA_DRIVER_OBJECT
    {
        ULONG DriverSize;
        BOOLEAN IsInitialized;
        PVOID DriverContext;
    } VEA_DRIVER_OBJECT, *PVEA_DRIVER_OBJECT;

    typedef enum _HARDWARE_STATE
    {
        HardwareStateDisabled = 0,
        HardwareStateEnabled  = 1,
        HardwareStateError    = 2
    } HARDWARE_STATE, *PHARDWARE_STATE;
    ```

    **Salah:**
    ```c
    // DILARANG: Tag tidak diawali '_', typedef tidak ALL_UPPER_CASE, dan field pakai camelCase/snake_case
    typedef struct vea_driver_object
    {
        ulong driver_size;     // SALAH: Tipe data & nama field snake_case
        BOOLEAN isInitialized; // SALAH: Field pakai camelCase
    } vea_driver_object;
    ```

*   **Tipe Dasar dalam Bahasa C:** Selalu menggunakan tipe dasar yang *explicit* seperti `ULONG`, `PULONG`, `VOID`, `PVOID`, `ULONG64`, `PULONG64`, dsb. Dilarang menggunakan tipe bawaan C biasa karena berpotensi rawan kesalahan jika VeaOS di-port ke mesin lain.

    **Benar:**
    ```c
    VOID
    VEAPI
    ProcessBuffer(
        IN PVOID BufferAddress,
        IN OUT ULONG BufferSize
    )
    {
        BOOLEAN IsValid = TRUE;
        ULONG Index = 0;
    }
    ```

    **Salah:**
    ```c
    void
    veapi
    ProcessBuffer(
        void *bufferAddress,    // DILARANG: Pakai void* bawaan C
        unsigned long bufferSize // DILARANG: Pakai unsigned long bawaan C
    )
    {
        bool isValid = true;    // DILARANG: Pakai bool C99
        int index = 0;          // DILARANG: Pakai int biasa
    }
    ```

*   **IN, OUT, OPTIONAL:** Fungsi yang menerima argumen, wajib menambahkan `IN`, `OUT`, `IN OUT`, atau `OPTIONAL` jika argumen yang dimasukkan ke fungsi diotak-atik oleh dalaman fungsi tersebut.

    **Benar:**
    ```c
    RETURN_TYPE
    CALLING_CONVENTION
    ApicReadRegister(
        IN ULONG RegisterOffset,
        OUT PULONG RegisterValue,
        IN OPTIONAL PVOID ExtraContext
    );
    ```

    **Salah:**
    ```c
    RETURN_TYPE
    CALLING_CONVENTION
    ApicReadRegister(
        ULONG RegisterOffset,   // SALAH: Tidak ada penanda IN / OUT
        PULONG RegisterValue,  // SALAH: Tidak jelas apakah variabel ini cuma dibaca atau diisi
        PVOID ExtraContext     // SALAH: Tidak ada penanda OPTIONAL
    );
    ```

*   **Bagian Atas File C dan H:**
    1. **SPDX License:** Wajib menaruh *license* dengan format berikut:
       ```c
       /* VeaOS SPDX License ----------------------------------------------------
         SPDX-License-Identifier: GPL-2.0-only
         LICENSE     : GNU General Public License v2.0
         FILE        : [NamaFile.c / NamaFile.h]
         CREATOR     : [Nama / Username Pembuat]
         MAINTAINER  : [Nama / Tim Pemelihara Kode
                        BISA NEW LINE. TAPI HARAP DIBUAT LIST DENGAN -]
         PURPOSE     : [Penjelasan singkat fungsi file ini. BISA NEW LINE]
        ----- Effective since MM-YYYY til FOREVER ------------------------------- */
       ```
    2. **Header:** 
       Untuk `.c`/`.cpp`, Header yang paling wajib untuk ditambah di atas adalah `veakrnl.h`. Lalu setelah itu, kalian bebas menambahkan Header yang kalian butuhkan.
       Untuk `.h`/`.hpp`, Header yang paling wajib ditambah di atas adalah `#pragma once` terlebih dahulu, loncat 1 line ke bawah, lalu `#include <procbind.h>` atau `#include "procbind.h"`, setelah itu tambahkan Header yang kalian butuhkan.
       
       **Contoh:**
       *.C:*
       ```c
       #include <veakrnl.h>
       #include <headergwganteng.h>
       ```
       *.H:*
       ```c
       #pragma once
       
       #include "procbind.h"
       #include "keterserahankalian.h"
       ```
    3. **Revision History:** Wajib menaruh Revision History di bawah header dengan format wajib sebagai berikut:
       ```c
       /* Revision History ------------------------------------------------------
        * Tanggal       : 26-07-2026
        * Nama Pengubah : Dev Ganteng
        * Revisi        : Menambahkan resetting TPR ke IRQL PASSIVE_LEVEL
        *
        * Tanggal       : 25-07-2026
        * Nama Pengubah : VeaOS Team
        * Revisi        : Inisialisasi awal fungsi ApicEnableLapic
        * --------------------------------------------------------------------- */
       ```

---

## 3. Aturan Formatting Ulang Kesalahan Format
Seandainya terdapat kesalahan *formatting* di file yang di-push (dari kalian), maka kalian wajib membenarkannya, ikuti pedoman *style coding* ini, dan *push* ulang dengan format ini:

`MODULE: FORMATTING | Pesan Commit`

---

## 4. Akhir Kata
File CODING STYLE ini berlaku mulai Sabtu 25 Juli 2026. File yang dibuat sebelum tanggal ini tidak wajib mengikuti aturan baku ini. Tetapi jika file yang dibuat sebelum tanggal 25 Juli 2026 direvisi seperti penambahan fungsi atau perubahan nama variable, harap terapkan aturan CODING STYLE ini ke revisi yang kalian lakukan, dan kalian tidak perlu mengubah/merevisi keseluruhan file.

Mudah-mudahan dengan penerapan CODING STYLE ini, *Source Code* VeaOS jadi lebih mudah dipahami dan dapat dicerna oleh orang awam.

Salam Hangat,
**#VEAOSTEAM**
