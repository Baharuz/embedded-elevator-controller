# Embedded Elevator Controller: Türkçe öğrenme rehberi

Bu rehber, projeyi kendi başına anlatabilmen, çalıştırabilmen ve değiştirebilmen için yazıldı. Önce sistemi bütün olarak gör; sonra fonksiyonlara in.

## 1. Projenin yaptığı iş

Program, 0–5 katlı **tek bir asansörü simüle eder**. Başlangıçta 1. katta bekler. Kullanıcı bir kat numarası girince istek FIFO kuyruğuna eklenir. Her 600 milisaniyede bir `elevator_tick` çağrılır. Bir tick, asansör hareket ediyorsa bir kat ilerleme; kapı açıksa kapı zamanlayıcısında bir adım anlamına gelir. Konsol bu sırada giriş almaya devam eder. `e` komutu motoru hemen durdurur, kuyruğu temizler ve acil durum LED bitini yakar.

Fiziksel asansörün motoru veya sensörü yoktur. `volatile uint8_t port` değişkeni, mikrodenetleyicideki 8 bitlik PORT register'ını **taklit eder**.

## 2. Dosyalar ve veri akışı

```text
Kullanıcı komutu ──> main.c ──> elevator_request / emergency / reset
                          │
                    her 600 ms tick
                          ↓
                    elevator.c (FSM + FIFO)
                          │
                    hw_* fonksiyonları
                          ↓
                    hardware.c ──> 8 bit PORT
```

- `include/hardware.h`: Register bit maskelerini ve driver fonksiyonlarını bildirir.
- `src/hardware.c`: Bitleri ayarlayan driver uygulamasıdır. Kat seçme kararı vermez.
- `include/elevator.h`: Durumları, sonuç kodlarını, `Elevator` yapısını ve controller API'sini tanımlar.
- `src/elevator.c`: Hangi durumda ne olacağına karar veren FSM ve istek kuyruğudur.
- `src/main.c`: Konsol, gerçek zaman saati, komut ayrıştırma ve durum çıktısıdır.
- `tests/test_elevator.c`: Fonksiyonları gerçek zaman beklemeden test eder.
- `Makefile`: GCC ile derleme, test ve çalıştırma hedefleridir.
- `.github/workflows/ci.yml`: GitHub'a push yapıldığında Ubuntu üzerinde derleme ve test yapar.

**Neden katmanlar ayrı?** `elevator.c`, `PORT` üzerindeki motor bitinin kaçıncı bit olduğunu bilmez; `hw_move_up` çağırır. Böylece ileride gerçek bir mikrodenetleyiciye geçerken donanım driver'ı değişir, karar mantığının büyük kısmı kalır. Gerçek donanım için ayrıca sensör ve güvenlik tasarımı gerekir.

## 3. C kavramları

### `enum`: aynı anda tek durum

`ElevatorState`, `IDLE`, `MOVING_UP`, `MOVING_DOWN`, `DOOR_OPEN`, `EMERGENCY` değerlerinden biridir. `switch (e->state)` mevcut duruma uygun işlemi seçer. `RequestResult` ise bir isteğin kabul, tekrar, geçersiz kat, acil durum veya dolu kuyruk sonucu verdiğini ayırır.

### `struct`: bir asansörün bütün verisi

`Elevator` içinde `current_floor`, `target_floor`, `door_ticks_left`, `state`, `queue`, `head`, `count` ve `hw` bulunur. Ayrı global değişkenler yerine hepsi tek nesnede tutulur. `Elevator *e` bu nesnenin adresidir; `e->state`, `(*e).state` ile aynı anlama gelir.

### İşaretçi ve `volatile`

`Hardware` içindeki `volatile uint8_t *port`, 8 bitlik register adresini tutar. `*hw->port` o adresteki değeri okur veya değiştirir. `volatile`, derleyiciye bu erişimleri gereksiz sayıp kaldırmamasını söyler. **Tek başına thread güvenliği veya atomiklik sağlamaz.** Bu projede bir konsol döngüsü kullanıldığı için ayrıca eşzamanlama yoktur.

### Bit maskeleri

`1u << 0` değeri `0000 0001`dir. `*port |= HW_MOTOR_UP` bit 0'ı 1 yapar. `*port &= 0xfcu` alt iki motor bitini temizler, diğer bitleri korur. `(*port & HW_MOTOR_UP) != 0` motor bitinin açık olup olmadığını okur. Üst dört bit, katı ikilik biçimde saklar: `floor << 4`.

## 4. PORT register haritası

```text
Bit:   7 6 5 4 | 3 | 2 | 1 | 0
       Kat     | E | K | A | Y
```

- 0: yukarı motoru (`Y`)
- 1: aşağı motoru (`A`)
- 2: kapı açık (`K`)
- 3: acil durum LED'i (`E`)
- 4–7: gösterilen kat

Örnek: `0x21 = 0010 0001`: üst kısım `0010`, yani kat 2; bit 0 açık, yani yukarı motoru çalışıyor. `0x34 = 0011 0100`: kat 3 ve kapı açık. `0x48 = 0100 1000`: kat 4, acil durum LED'i açık, motorlar kapalı.

`hw_move_up` önce `hw_stop` ile iki motor bitini temizler, sonra kapıyı kapatır ve yukarı bitini açar. `open_at_target` önce motoru durdurur, sonra kapıyı açar. Bunlar simülasyonun önemli değişmezleridir: **hareket ve açık kapı bir arada olmamalı; iki motor aynı anda açık olmamalı.**

## 5. FSM'i adım adım oku

| Mevcut durum | Olay/tick | Sonraki durum | İşlem |
| --- | --- | --- | --- |
| `IDLE` | İstek yok | `IDLE` | Bekle |
| `IDLE` | Hedef yukarıda | `MOVING_UP` | Yukarı motorunu aç |
| `IDLE` | Hedef aşağıda | `MOVING_DOWN` | Aşağı motorunu aç |
| `IDLE` | Hedef mevcut kat | `DOOR_OPEN` | Kapıyı aç |
| `MOVING_UP` / `MOVING_DOWN` | Hedefe ulaşılmadı | Aynı durum | Bir kat ilerle |
| `MOVING_UP` / `MOVING_DOWN` | Hedefe ulaşıldı | `DOOR_OPEN` | Motoru durdur, kapıyı aç |
| `DOOR_OPEN` | Sayaç > 0 | `DOOR_OPEN` | Sayacı azalt |
| `DOOR_OPEN` | Sayaç 0 | `IDLE` | Kapıyı kapat |
| Herhangi bir durum | `e` komutu | `EMERGENCY` | Motoru durdur, kuyruğu temizle, LED'i yak |
| `EMERGENCY` | `r` komutu | `IDLE` | LED'i söndür, mevcut katta bekle |

`elevator_tick` içindeki `switch`, tabloda gördüğün geçişlerin kod karşılığıdır. Hareket sırasında fiziksel sensör okunmaz; `current_floor` yazılımla bir artırılır veya azaltılır. Kat sınırında yanlış yönde hareket tespit edilirse acil duruma geçilir.

### Örnek: 1. kattan 3. kata

1. Başlangıç `IDLE`, kat 1, PORT `0x10`.
2. `3` komutu: `elevator_request` kuyruğa 3 ekler; henüz motor çalışmaz.
3. İlk tick: kuyruktan 3 alınır, hedef 3 olur, `MOVING_UP`, PORT `0x11`.
4. İkinci tick: kat 2, PORT `0x21`.
5. Üçüncü tick: kat 3, motor kapanır, kapı açılır, `DOOR_OPEN`, PORT `0x34`.
6. Sonraki iki tick: kapı sayacı 2→1→0; kapı kapanır, `IDLE`, PORT `0x30`.

Hedef kat şu anki kat ise hareket olmadan kapı açılır. Hedef aşağıdaysa aynı akış `MOVING_DOWN` ile ters yönde işler.

## 6. FIFO kuyruk nasıl çalışır?

`queue` altı elemanlı dairesel dizidir. `head`, sıradaki okunacak elemanın indeksidir. `count`, bekleyen istek sayısıdır. Yeni elemanın yeri `(head + count) % 6` ile bulunur. Okuma sonrası `head = (head + 1) % 6` olur. `% 6`, son indeksten tekrar başa dönmeyi sağlar.

Örnek: Önce `3`, sonra `0` istenirse önce 3'e, kapı kapandıktan sonra 0'a gidilir. Kuyruk hareket sırasında da yeni istek kabul eder. Aynı hedef veya kuyruktaki aynı kat tekrar eklenmez. FIFO yön optimizasyonu yapmaz; arada kalan katta otomatik durmaz.

## 7. Konsol ve zamanlama

`main.c` girişi sürekli yoklar. Windows'ta `_kbhit()` ve `_getch()`, Linux'ta `select()` ve `fgets()` kullanılır. `now_ms()` Windows'ta `GetTickCount64`, Linux'ta `clock_gettime(CLOCK_MONOTONIC)` çağırır. Süre dolunca `elevator_tick` çalışır. Böylece uzun, bloklayan bir “hedefe varana kadar çalış” döngüsü yoktur; hareket sırasında `e` veya başka kat girebilirsin.

600 ms bir simülasyon seçimidir; gerçek asansör fizik modeli değildir. Tick hızını `src/main.c` içindeki `TICK_MS` değiştirir. Kapı süresini `include/elevator.h` içindeki `ELEVATOR_DOOR_TICKS` belirler.

## 8. Bilgisayarında çalıştırma

### Ubuntu / GCC

Proje klasöründe terminal aç:

```sh
sudo apt update
sudo apt install build-essential
make
make test
make run
```

`make` simülatörü derler; `make test` testleri derleyip çalıştırır; `make run` simülatörü açar. `make clean` derleme çıktılarını siler.

Make kullanmadan GCC ile:

```sh
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude src/main.c src/elevator.c src/hardware.c -o elevator_sim
./elevator_sim
```

### Bu Windows bilgisayarındaki Visual Studio C derleyicisi

**Developer PowerShell for VS 2022** aç, proje klasörüne geç ve çalıştır:

```powershell
cd 'C:\Users\2006b\Documents\Codex\2026-10-16\typedef-enum-x20-idle-bekleme-x20\outputs\embedded-elevator-controller'
cl /nologo /W4 /WX /std:c11 /Iinclude src\main.c src\elevator.c src\hardware.c /Fe:elevator_sim.exe
.\elevator_sim.exe
```

Testleri çalıştırmak için:

```powershell
cl /nologo /W4 /WX /std:c11 /Iinclude tests\test_elevator.c src\elevator.c src\hardware.c /Fe:test_elevator.exe
.\test_elevator.exe
```

Bu bilgisayardaki `C:\MinGW\bin\gcc.exe` bozuk DLL hatası verdiği için Windows'ta Visual Studio derleyicisini kullandım. GitHub Actions, Ubuntu üzerinde GCC ile `make` ve `make test` çalıştırdı ve başarılı oldu.

### Konsolda dene

`3` + Enter ile 3. kata git. Giderken `0` + Enter ile yeni istek ekle. Hareket sırasında `e` + Enter ile acil durdur; PORT'ta motor bitlerinin sıfırlandığını gör. `2` + Enter isteği acil durumda reddedilir. `r` + Enter ile resetle, `v` + Enter ile bitleri gör, `q` + Enter ile çık.

## 9. Testleri anla

Testler `assert` kullanır. İfade yanlışsa test programı hata koduyla durur. `test_init_and_invalid` başlangıç ve geçersiz girişi; `test_up_down_and_queue` iki yönde hareket ve kapıyı; `test_emergency_and_reset` motor durdurma, LED ve reseti; `test_same_floor_and_capacity` aynı kat ile kuyruğu kontrol eder. Testler `elevator_tick` fonksiyonunu doğrudan çağırır; 600 ms beklemez. Bu sayede hızlı ve tekrarlanabilirdir.

## 10. Sınırlar ve geliştirme fikirleri

- Gerçek kat sensörü ve kapı sensörü yoktur; fiziksel donanıma doğrudan bağlanmamalıdır.
- Acil durum sonrası `r`, operatör onayı gibi simüle edilir; gerçek sistemde daha güçlü koşullar gerekir.
- FIFO, hareket yönüne göre durakları optimize etmez.
- Konsol girdisi çok uzun veya bozuksa kullanıcı deneyimi geliştirilebilir.
- İyi sonraki çalışma: `elevator_tick` için bit değişmezlerini her adımda kontrol eden ek testler, sensör arayüzü, yön bazlı kat seçimi ve çoklu asansör simülasyonu.

## 11. Kendini sınamak için sorular

1. `0x24` ne anlatır? (Kat 2, kapı açık.)
2. Neden `hw_move_up` önce `hw_stop` çağırıyor? (İki motor biti aynı anda açık kalmasın.)
3. Neden `elevator_request` motoru hemen açmıyor? (Olayı kuyruğa yazar; durum geçişi tick sırasında yapılır.)
4. `head` 5 iken bir istek çıkarılırsa yeni `head` kaç olur? (`(5+1)%6 = 0`.)
5. `volatile` neden mutex yerine geçmez? (Erişimi korumaz; yalnızca derleyicinin bazı optimizasyonlarını sınırlar.)
6. `3` isteğinde ilk tick neden hâlâ kat 1 gösteriyor? (O tick motoru başlatır; sonraki tick katı ilerletir.)

Bu soruları kodu açıp ilgili satırları bularak cevaplayabiliyorsan projenin temel mantığını kavramışsın demektir.
