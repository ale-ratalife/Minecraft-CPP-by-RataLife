# MineCPP

Un motor de vóxeles y clon de Minecraft ligero y procedural, escrito desde cero en C++ puro usando la API de Win32 y OpenGL. 

Desarrollado por RataLife (V. Alejandro Téllez / TwentyFox), MineCPP destaca por no depender de assets externos: todas las texturas del mundo se generan proceduralmente en tiempo de ejecución mediante algoritmos de ruido y matemáticas.

## Características Principales

* **Generación Procedural Avanzada:** Terreno dinámico usando algoritmos de ruido (Perlin, FBM, Ridged). Generación de montañas, ríos, planicies y cuevas.
* **Texturas Generadas por Código:** Cero dependencias de archivos `.png` o `.jpg`. El atlas de texturas se pinta pixel por pixel al iniciar el juego utilizando matemáticas (AtlasGen).
* **Shaders Personalizados (GLSL):** Soporte para shaders de vértices y fragmentos. Incluye renderizado de agua con oleaje y destellos dinámicos, además de niebla atmosférica.
* **Sistema de Chunks Infinitos:** Carga y descarga dinámica de chunks alrededor del jugador basada en la distancia de renderizado.
* **Físicas y Fluidos:** Sistema de colisiones AABB, gravedad, mecánicas de jugador (caminar, correr, agacharse, volar) y propagación de agua y lava.
* **Inventario y Crafteo:** Sistema básico de recolección de bloques, inventario de 36 slots y recetas de crafteo básicas.

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

* Sistema Operativo: Windows 10 / 11
* Compilador: MSVC (Visual Studio 2019/2022) o MinGW
* Bibliotecas enlazadas: `opengl32.lib`, `user32.lib`, `gdi32.lib`

**Para compilar en Visual Studio:**
1. Crea un proyecto vacío de C++ (Aplicación de Windows).
2. Agrega todos los archivos `.cpp` y `.h` al proyecto.
3. Asegúrate de que el subsistema esté configurado en **Windows** (`/SUBSYSTEM:WINDOWS`).
4. Compila en modo Release (x64) para un rendimiento óptimo.

## Estructura del Código

* `MineCPP.cpp`: Punto de entrada (WinMain), bucle principal, manejo de ventana (Win32), input y renderizado de la UI.
* `World.cpp` / `World.h`: Gestión de chunks, voxel data, generación de mallas (meshing) y físicas de fluidos.
* `Noise.h`: Funciones matemáticas rápidas de ruido Perlin, FBM y hash.
* `Atlas.h`: Generador de texturas procedurales (Pixel Art generado por código).
* `shaders_gl2.h` / `shaders_src.h`: Carga dinámica de extensiones modernas de OpenGL y código fuente de los shaders GLSL.

## Créditos

Desarrollado por **V. Alejandro Téllez** (RataLife / TwentyFox).
