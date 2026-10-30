<div align="center">
  <a href="#español">Español</a> •
  <a href="#english">English</a> •
  <a href="#indonesia">Indonesia</a>
</div>

---

# <a id="español"></a> MineCPP (Español)

Un motor de vóxeles y clon de Minecraft ligero y procedural, escrito desde cero en C++ puro usando la API de Win32 y OpenGL. 

Desarrollado por RataLife (V. Alejandro Téllez), MineCPP destaca por no depender de assets externos: todas las texturas del mundo se generan proceduralmente en tiempo de ejecución mediante algoritmos de ruido y matemáticas.

## Características Principales

- **Generación Procedural Avanzada:** Terreno dinámico usando algoritmos de ruido (Perlin, FBM, Ridged). Generación de montañas, ríos, planicies y cuevas.
- **Texturas Generadas por Código:** Cero dependencias de archivos `.png` o `.jpg`. El atlas de texturas se pinta pixel por pixel al iniciar el juego utilizando matemáticas (AtlasGen).
- **Shaders Personalizados (GLSL):** Soporte para shaders de vértices y fragmentos. Incluye renderizado de agua con oleaje y destellos dinámicos, además de niebla atmosférica.
- **Sistema de Chunks Infinitos:** Carga y descarga dinámica de chunks alrededor del jugador basada en la distancia de renderizado.
- **Físicas y Fluidos:** Sistema de colisiones AABB, gravedad, mecánicas de jugador (caminar, correr, agacharse, volar) y propagación de agua y lava.
- **Inventario y Crafteo:** Sistema básico de recolección de bloques, inventario de 36 slots y recetas de crafteo básicas.

## Controles

| Tecla / Botón | Acción |
| :--- | :--- |
| **W, A, S, D** | Moverse |
| **Espacio** | Saltar (Doble toque en Creativo para volar) |
| **Shift Izquierdo** | Agacharse / Bajar (volando) |
| **Ctrl Izquierdo** | Correr (Sprint) |
| **Click Izquierdo** | Romper bloque |
| **Click Derecho** | Colocar bloque |
| **Rueda del Ratón / 1-9** | Cambiar objeto en la barra de acceso rápido |
| **E** | Abrir/Cerrar Inventario y Crafteo |
| **F11** | Alternar Pantalla Completa |
| **Esc** | Pausar juego |
| **+ / -** | Aumentar o disminuir la distancia de renderizado |

## Requisitos de Compilación

El proyecto está diseñado para compilarse nativamente en Windows sin bibliotecas externas masivas.

- Sistema Operativo: Windows 10 / 11
- Compilador: MSVC (Visual Studio 2019/2022) o MinGW
- Bibliotecas enlazadas: `opengl32.lib`, `user32.lib`, `gdi32.lib`

**Para compilar en Visual Studio:**
1. Crea un proyecto vacío de C++ (Aplicación de Windows).
2. Agrega todos los archivos `.cpp` y `.h` al proyecto.
3. Asegúrate de que el subsistema esté configurado en **Windows** (`/SUBSYSTEM:WINDOWS`).
4. Compila en modo Release (x64) para un rendimiento óptimo.

## Estructura del Código

- `MineCPP.cpp`: Punto de entrada (WinMain), bucle principal, manejo de ventana (Win32), input y renderizado de la UI.
- `World.cpp` / `World.h`: Gestión de chunks, voxel data, generación de mallas (meshing) y físicas de fluidos.
- `Noise.h`: Funciones matemáticas rápidas de ruido Perlin, FBM y hash.
- `Atlas.h`: Generador de texturas procedurales (Pixel Art generado por código).
- `shaders_gl2.h` / `shaders_src.h`: Carga dinámica de extensiones modernas de OpenGL y código fuente de los shaders GLSL.

## Créditos

Desarrollado por **V. Alejandro Téllez** (RataLife).

<br>

---

# <a id="english"></a> MineCPP (English)

A lightweight, procedural voxel engine and Minecraft clone, written from scratch in pure C++ using the Win32 API and OpenGL.

Developed by RataLife (V. Alejandro Téllez), MineCPP stands out for not depending on external assets: all world textures are generated procedurally at runtime using noise algorithms and mathematics.

## Main Features

- **Advanced Procedural Generation:** Dynamic terrain using noise algorithms (Perlin, FBM, Ridged). Generation of mountains, rivers, plains, and caves.
- **Code-Generated Textures:** Zero `.png` or `.jpg` file dependencies. The texture atlas is painted pixel by pixel upon game startup using mathematics (AtlasGen).
- **Custom Shaders (GLSL):** Support for vertex and fragment shaders. Includes water rendering with waves and dynamic sparkles, plus atmospheric fog.
- **Infinite Chunk System:** Dynamic loading and unloading of chunks around the player based on render distance.
- **Physics and Fluids:** AABB collision system, gravity, player mechanics (walking, sprinting, crouching, flying), and water/lava propagation.
- **Inventory and Crafting:** Basic block gathering system, 36-slot inventory, and basic crafting recipes.

## Controls

| Key / Button | Action |
| :--- | :--- |
| **W, A, S, D** | Move |
| **Space** | Jump (Double tap in Creative to fly) |
| **Left Shift** | Crouch / Descend (while flying) |
| **Left Ctrl** | Sprint |
| **Left Click** | Break block |
| **Right Click** | Place block |
| **Mouse Wheel / 1-9** | Change item in hotbar |
| **E** | Open/Close Inventory and Crafting |
| **F11** | Toggle Fullscreen |
| **Esc** | Pause game |
| **+ / -** | Increase or decrease render distance |

## Build Requirements

The project is designed to compile natively on Windows without massive external libraries.

- Operating System: Windows 10 / 11
- Compiler: MSVC (Visual Studio 2019/2022) or MinGW
- Linked Libraries: `opengl32.lib`, `user32.lib`, `gdi32.lib`

**To compile in Visual Studio:**
1. Create an empty C++ project (Windows Application).
2. Add all `.cpp` and `.h` files to the project.
3. Ensure the subsystem is set to **Windows** (`/SUBSYSTEM:WINDOWS`).
4. Compile in Release mode (x64) for optimal performance.

## Code Structure

- `MineCPP.cpp`: Entry point (WinMain), main loop, window management (Win32), input, and UI rendering.
- `World.cpp` / `World.h`: Chunk management, voxel data, mesh generation (meshing), and fluid physics.
- `Noise.h`: Fast mathematical functions for Perlin noise, FBM, and hash.
- `Atlas.h`: Procedural texture generator (Code-generated Pixel Art).
- `shaders_gl2.h` / `shaders_src.h`: Dynamic loading of modern OpenGL extensions and GLSL shader source code.

## Credits

Developed by **V. Alejandro Téllez** (RataLife).

<br>

---

# <a id="indonesia"></a> MineCPP (Indonesia)

Mesin voxel prosedural dan klon Minecraft yang ringan, ditulis dari awal dalam C++ murni menggunakan Win32 API dan OpenGL.

Dikembangkan oleh RataLife (V. Alejandro Téllez), MineCPP menonjol karena tidak bergantung pada aset eksternal: semua tekstur dunia dibuat secara prosedural saat runtime menggunakan algoritma noise dan matematika.

## Fitur Utama

- **Generasi Prosedural Lanjutan:** Medan dinamis menggunakan algoritma noise (Perlin, FBM, Ridged). Pembuatan gunung, sungai, dataran, dan gua.
- **Tekstur Buatan Kode:** Nol dependensi file `.png` atau `.jpg`. Atlas tekstur dilukis piksel demi piksel saat permainan dimulai menggunakan matematika (AtlasGen).
- **Shader Kustom (GLSL):** Dukungan untuk vertex dan fragment shader. Termasuk rendering air dengan gelombang dan kilauan dinamis, serta kabut atmosfer.
- **Sistem Chunk Tak Terbatas:** Pemuatan dan pembongkaran chunk secara dinamis di sekitar pemain berdasarkan jarak render.
- **Fisika dan Cairan:** Sistem tabrakan AABB, gravitasi, mekanika pemain (berjalan, berlari, merunduk, terbang), dan penyebaran air/lava.
- **Inventaris dan Crafting:** Sistem pengumpulan blok dasar, inventaris 36 slot, dan resep crafting dasar.

## Kontrol

| Tombol / Klik | Aksi |
| :--- | :--- |
| **W, A, S, D** | Bergerak |
| **Spasi** | Lompat (Ketuk ganda di mode Kreatif untuk terbang) |
| **Shift Kiri** | Merunduk / Turun (saat terbang) |
| **Ctrl Kiri** | Lari Cepat (Sprint) |
| **Klik Kiri** | Hancurkan blok |
| **Klik Kanan** | Tempatkan blok |
| **Roda Mouse / 1-9** | Ubah item di hotbar |
| **E** | Buka/Tutup Inventaris dan Crafting |
| **F11** | Beralih Layar Penuh |
| **Esc** | Jeda permainan |
| **+ / -** | Tambah atau kurangi jarak render |

## Persyaratan Kompilasi

Proyek ini dirancang untuk dikompilasi secara asli di Windows tanpa perpustakaan eksternal yang besar.

- Sistem Operasi: Windows 10 / 11
- Kompilator: MSVC (Visual Studio 2019/2022) atau MinGW
- Perpustakaan Tertaut: `opengl32.lib`, `user32.lib`, `gdi32.lib`

**Untuk mengkompilasi di Visual Studio:**
1. Buat proyek C++ kosong (Aplikasi Windows).
2. Tambahkan semua file `.cpp` dan `.h` ke dalam proyek.
3. Pastikan subsistem diatur ke **Windows** (`/SUBSYSTEM:WINDOWS`).
4. Kompilasi dalam mode Release (x64) untuk performa optimal.

## Struktur Kode

- `MineCPP.cpp`: Titik masuk (WinMain), loop utama, manajemen jendela (Win32), input, dan rendering UI.
- `World.cpp` / `World.h`: Manajemen chunk, data voxel, pembuatan mesh (meshing), dan fisika cairan.
- `Noise.h`: Fungsi matematika cepat untuk noise Perlin, FBM, dan hash.
- `Atlas.h`: Generator tekstur prosedural (Seni Piksel yang dihasilkan oleh kode).
- `shaders_gl2.h` / `shaders_src.h`: Pemuatan dinamis ekstensi OpenGL modern dan kode sumber shader GLSL.

## Kredit

Dikembangkan oleh **V. Alejandro Téllez** (RataLife).
