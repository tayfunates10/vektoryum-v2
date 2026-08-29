# Vektoryum v2

> **Durum:** Core Engine geliştirme aşaması 0/13 tamamlandı — **%4**
> **Aktif aşama:** Aşama 1 — Repository Foundation & CI
> **Çalışma modeli:** Branch → Pull Request → test/CI → yalnızca yeşilse `main` merge → sonraki aşama

Vektoryum v2; logo, çizim, ikon, illüstrasyon, ekran grafiği ve fotoğraf gibi farklı raster görselleri içerik türüne göre analiz eden; mümkün olan en yüksek algısal ve geometrik sadakatle büyüten, gerektiğinde yeniden yapılandıran ve vektörleştirilebilir içerikleri gerçek vektör geometrisine dönüştüren profesyonel bir görüntü işleme motorudur.

## 1. Ürün hedefi

Core Engine tamamlandığında yazılım:

- Raster görselleri 2×, 4×, 8× ve kontrollü özel ölçeklerde büyütecek.
- Büyütme sırasında aliasing, ringing, halo, bloklaşma, renk kayması, alfa kenarı kirlenmesi ve yapay keskinleştirmeyi ölçüp engelleyecek.
- Fotoğraf ile logo/çizim/ikon gibi içerikleri aynı algoritmaya zorlamayacak; içerik türünü analiz edip uygun işleme hattını seçecek.
- Logo/ikon/çizgi sanatında kontur, köşe, eğri, dolgu, stroke, alfa ve katman ilişkilerini yeniden kurarak **gerçek SVG** üretecek.
- Fotoğrafta kayıp ayrıntıyı “varmış gibi” kesin gerçek kabul etmeyecek; rekonstrüksiyon/generative sonuçları ölçülebilir güven bilgisiyle ayıracak.
- PNG ve yüksek kaliteli raster çıktılar ile SVG başta olmak üzere desteklenen vektör çıktılar sağlayacak.
- Aynı girdi + aynı ayarlarda deterministik/tekrarlanabilir sonuç üretebilecek.
- Büyük görsellerde tile/overlap işleme ile bellek taşmasını önleyecek.
- Core Engine; ileride eklenecek web arayüzü, kullanıcı hesabı ve abonelik sisteminden bağımsız olacak.

### Bilimsel kalite ilkesi

Düşük çözünürlüklü bir rasterda hiç kaydedilmemiş gerçek dünya detayını matematiksel olarak yüzde 100 geri getirmek her görüntü için mümkün değildir. Bu nedenle “kusursuz” hedefi; **uydurulmuş ayrıntıyı gerçek diye sunmak değil**, ölçülebilir bozulmayı en aza indirmek, bilinen yapısal ayrıntıyı korumak ve vektörleştirilebilir içerikte geometrik doğruluğu maksimuma çıkarmaktır.

## 2. Bağımsızlık / kendi motorumuzu yazma politikası

Vektoryum v2’nin üretim çekirdeği başka bir super-resolution, vectorizer veya hazır AI modelini sarmalamayacaktır.

**Yasak:**
- Real-ESRGAN, ESRGAN, waifu2x, Stable Diffusion upscaler, OpenCV super-resolution/vectorizer benzeri hazır motorları üretim algoritması olarak çağırmak.
- Potrace/AutoTrace/Vectorizer.ai vb. bir vektörleştirme motorunu arka planda kullanmak.
- Başka projelerin eğitilmiş ağırlıklarını kopyalamak veya indirip üretimde kullanmak.
- Kalite testini geçmek için eşikleri düşürmek.

**İzinli altyapı sınırı:**
- Dilin standart kütüphanesi, işletim sistemi API’leri, derleyici/toolchain ve dosya biçimi spesifikasyonları kullanılabilir.
- Gerektiğinde düşük seviyeli sayısal/GPU altyapısı kullanılabilir; ancak görüntü rekonstrüksiyonu, vektörizasyon mantığı, model mimarisi, eğitim hedefleri ve Vektoryum’a ait ağırlıklar bu repoda geliştirilecektir.
- Üçüncü taraf bağımlılık eklenirse üretim algoritması olmadığı kanıtlanmalı, lisansı kaydedilmeli ve mimari kararda gerekçelendirilmelidir.

## 3. Mimari hedef

```text
Input
  ↓
Decode + Metadata + Color/Alpha normalization
  ↓
Content Analyzer / Router
  ├─ Photo / natural image ───────→ Restoration + SR pipeline
  ├─ Logo / icon / line-art ──────→ Geometry + color-region reconstruction
  ├─ Mixed content ───────────────→ Region-aware hybrid pipeline
  └─ Unsupported/uncertain ───────→ Conservative high-fidelity raster path
                                  ↓
                         Quality Evaluator
                     ↙ raster              vector ↘
             PNG/TIFF/etc.                 SVG/etc.
```

Core modüller planı:

```text
vektoryum/
  core/            # görüntü, renk, alfa, tile, koordinat ve matematik tipleri
  io/              # güvenli decode/encode ve metadata
  analysis/        # içerik sınıflandırma / özellik çıkarımı / routing
  resample/        # kendi ölçekleme ve reconstruction algoritmaları
  restore/         # noise, blur, compression ve edge-aware restoration
  vector/          # segmentation, contour, topology, curve fitting, SVG scene graph
  ml/              # Vektoryum model/autodiff/runtime ve inference katmanı
  quality/         # metrikler, evaluator, acceptance gates
  export/          # raster/vector output writers
  cli/             # headless kullanım
  api/             # sonraki UI için stabil core API yüzeyi
benchmarks/
tests/
tools/
docs/
```

## 4. Bitirme yol haritası — toplam %100

Yüzdeler iş yükü ve ürün riskine göre ağırlıklandırılmıştır. Bir aşamanın yüzdesi **kod yazıldığı için değil, acceptance kriterleri ve CI tamamen geçtiğinde** kazanılır.

| # | Aşama | Ağırlık | Bitiş kriteri | Durum |
|---|---|---:|---|---|
| 0 | Ürün şartnamesi, kalite ilkeleri, yol haritası | **4%** | README + kalite politikası + PR/CI çalışma kuralı | ✅ |
| 1 | Repository Foundation & CI | **6%** | Proje iskeleti, format/lint, unit test runner, sanitizers, CI matrisi | ⏳ |
| 2 | Image Core: renk, alfa, tile, I/O sözleşmeleri | **8%** | Bit-depth/alpha/color invariants + tile seam testleri | ⬜ |
| 3 | Kendi analitik yüksek kaliteli raster resampler motoru | **8%** | 2×/4×/8×; edge/ringing/aliasing kalite kapıları | ⬜ |
| 4 | Content Analyzer & deterministic router | **5%** | photo/logo/line-art/mixed ayrımı + confidence + fallback | ⬜ |
| 5 | Logo/çizim için gerçek vector reconstruction engine | **12%** | topology, contour, corner, Bézier, fill/stroke, alpha + SVG | ⬜ |
| 6 | Photo restoration + non-ML super-resolution çekirdeği | **12%** | blur/noise/JPEG/edge test setlerinde baseline üstünlüğü | ⬜ |
| 7 | Vektoryum’a ait ML/DL model/runtime katmanı | **14%** | kendi mimari/ağırlıklarımız, inference, determinism, model tests | ⬜ |
| 8 | Eğitim/veri/benchmark üretim hattı | **7%** | rights-clean veri manifesti, degradation generator, reproducibility | ⬜ |
| 9 | Hybrid reconstruction ve kaliteye göre füzyon | **7%** | region-aware seçim; kalite kötüleşirse güvenli rollback | ⬜ |
| 10 | Üretim exporter’ları: SVG + lossless/high-quality raster | **5%** | round-trip, metadata, alpha, malformed-output testleri | ⬜ |
| 11 | Geniş kalite, fuzz, adversarial, performans ve bellek sertifikasyonu | **6%** | bütün zorunlu kalite kapıları yeşil | ⬜ |
| 12 | Stabil CLI/Core API ve entegrasyon sözleşmesi | **3%** | versioned API, batch mode, errors, cancellation, progress | ⬜ |
| 13 | Release hardening, dokümantasyon ve reproducible release | **3%** | release checklist + temiz kurulumdan doğrulanmış build | ⬜ |
| | **CORE ENGINE TOPLAM** | **100%** | | **%4** |

> **UI + kullanıcı hesabı + abonelik/ödeme sistemi bu %100 Core Engine hesabına dahil değildir.** Core Engine %100 olduktan ve kalite sertifikasyonu geçtikten sonra ayrı “Product Layer” yol haritası açılacaktır.

## 5. Kalite kapıları

Her PR için kapsamına göre aşağıdaki testler zorunlu hale getirilecektir.

### 5.1 Kod doğruluğu
- Unit test
- Integration test
- Property-based test
- Regression test
- Golden/master fixture test
- Invalid/corrupt input test
- Determinism test
- Cross-platform/build matrix

### 5.2 Görsel kalite
Raster kalite evaluator en az şu ölçümleri kullanacaktır:
- PSNR
- SSIM / multi-scale structural similarity yaklaşımı
- edge preservation error
- gradient magnitude/orientation error
- chroma/luma drift
- ringing/halo score
- aliasing score
- alpha-edge contamination
- tile seam score
- texture consistency

Tek bir metrik başarı kabulü için yeterli olmayacaktır. Bazı metrikler referanslı, bazıları referanssız kalite kontrolü olarak birlikte değerlendirilecektir.

### 5.3 Vektör kalite
- Rasterize-back pixel agreement
- alpha IoU / mask agreement
- contour distance (Hausdorff/Chamfer sınıfı ölçümler)
- corner retention
- topology / hole / connected-component preservation
- self-intersection ve invalid path kontrolü
- node/segment complexity explosion kontrolü
- SVG parse/round-trip
- farklı rasterizer’larda tutarlılık

### 5.4 Dayanıklılık ve güvenlik
- Fuzz test
- malformed image corpus
- decompression-bomb / aşırı boyut koruması
- integer overflow / bounds checks
- memory sanitizer / address sanitizer uygunluğu
- timeout/cancellation
- maksimum bellek bütçesi

### 5.5 Performans
Her release candidate için:
- 1 MP, 4 MP, 12 MP ve büyük görsel benchmarkları
- 2×/4×/8× süre ve peak RAM
- tile scaling efficiency
- cold/warm run karşılaştırması
- CPU baseline; uygun aşamada GPU backend

Performans kazanmak için kalite kapısı gevşetilmeyecektir.

## 6. Test görüntü sınıfları

Benchmark tek tip görsele göre optimize edilmeyecektir:

1. Flat-color logo
2. Transparent logo / alpha edge
3. İnce çizgili line-art
4. Küçük ikon / pixel art (ayrı politika)
5. Metin içeren UI/screenshot
6. Gradient / soft shadow grafik
7. Anime/illustration benzeri sentetik çizim
8. Portre fotoğrafı
9. Saç/kürk/ince doku
10. Mimari / sert geometrik kenarlar
11. Doğa / foliage / stochastic texture
12. JPEG sıkıştırılmış düşük kalite fotoğraf
13. Motion/defocus blur
14. Gürültülü düşük ışık
15. Mixed photo + typography + logo

Her sınıf için clean reference → kontrollü degradation → reconstruction → ölçüm zinciri kurulacaktır.

## 7. CI ve PR politikası

`main` her zaman release-adayı kalitede tutulacaktır.

1. Her teknik aşama ayrı branch üzerinde geliştirilir.
2. PR açıklamasında amaç, değişiklikler, riskler, testler ve yüzdelik etkisi yazılır.
3. CI başarısızsa merge yapılmaz.
4. Başarısız test düzeltilir; test/eşik silinmez veya gevşetilmez.
5. CI tamamen yeşil + zorunlu acceptance kriterleri geçerse PR `main`’e merge edilir.
6. Merge sonrası README ilerleme yüzdesi ve aktif aşama güncellenir.
7. Sonraki aşamaya ancak bundan sonra geçilir.

### Merge için minimum şart

```text
build              PASS
unit               PASS
integration        PASS
quality-regression PASS
security/fuzz*     PASS (aşama kapsamında zorunluysa)
benchmark-budget*  PASS (aşama kapsamında zorunluysa)
review blockers    0
```

## 8. Definition of Done

Core Engine ancak aşağıdakilerin **tamamı** sağlanırsa %100 kabul edilir:

- Yol haritasındaki 0–13 aşamalarının tamamı merge edilmiştir.
- Zorunlu CI kontrolleri yeşildir.
- Bilinen kritik/yüksek seviye hata yoktur.
- Logo/line-art vektör hattı gerçek SVG geometrisi üretir; rasterı SVG içine gömmek başarı sayılmaz.
- Büyük görüntüler tile seam oluşturmadan işlenebilir.
- Corrupt/adversarial girdiler kontrollü hata verir; crash/UB kabul edilmez.
- Aynı model/sürüm/ayar ile tekrarlanabilir sonuç alınabilir.
- Kalite benchmark raporu release artifact olarak üretilebilir.
- Temiz bir makinede dokümante edilmiş komutlarla build/test yapılabilir.
- Core API, ilerideki UI/abonelik katmanını çekirdeğe bağımlı kılmadan entegre edilebilir.

## 9. Sonraki ürün katmanı — Core %100 sonrası

Core tamamlandıktan sonra ayrı bir roadmap ile:
- profesyonel web/desktop arayüz,
- kullanıcı hesabı,
- abonelik seviyeleri,
- ödeme sağlayıcısı,
- job queue,
- kullanım kotası/credit,
- güvenli dosya saklama/silme,
- admin/observability,
- production deployment

geliştirilecektir. Bu katman Core Engine kalite hedeflerini değiştirmeyecektir.

## 10. İlerleme kaydı

| Tarih | Olay | Core ilerleme |
|---|---|---:|
| 2026-08-29 | Repo bootstrap edildi; ürün kapsamı, kalite politikası ve 13 aşamalı teknik yol haritası oluşturuldu. | **%4** |

---

**Sıradaki iş:** Aşama 1 — proje iskeleti, build sistemi, test altyapısı, CI matrisi ve ilk kalite kapılarını PR üzerinden kurmak.
