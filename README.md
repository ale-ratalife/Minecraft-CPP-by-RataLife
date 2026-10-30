MineCPP ⛏️
MineCPP es un clon ligero y autónomo de Minecraft escrito completamente desde cero en C++ y OpenGL. Desarrollado por RataLife, este proyecto es un motor voxel nativo para Windows que no depende de motores gráficos externos ni de bibliotecas pesadas de ventanas (como GLFW o SDL). Todo, desde el contexto de la ventana hasta la carga de extensiones OpenGL, está hecho a medida utilizando la API de Win32.
¡Incluso las texturas del juego se generan de forma procedural mediante código en tiempo de ejecución! No requiere assets externos.
✨ Características Principales
Generación de Mundo Procedural: Terreno infinito generado mediante múltiples capas de ruido Perlin y Simplex (montañas, valles, océanos y cuevas).
Texturizado Procedural (`Atlas.h`): Las texturas de los bloques (pasto, tierra, piedra, agua, etc.) se pintan pixel por pixel al iniciar el juego utilizando algoritmos matemáticos y ruido.
Shaders GLSL Personalizados: Efectos visuales de niebla atmosférica y simulación de ondas de agua animadas mediante vertex y fragment shaders.
Dinámica de Fluidos: El agua y la lava fluyen, empujan al jugador y reaccionan al entorno de forma realista.
Sistema de Chunks y Culling: El mundo se divide en chunks (16x128x16) utilizando un sistema de mallas eficiente (Display Lists) para renderizar solo las caras visibles de los bloques.
Modos de Juego:
Supervivencia: Salud, respiración bajo el agua, daño por caída/lava, e inventario funcional.
Creativo: Vuelo libre, invulnerabilidad y recursos ilimitados.
Interfaz Completa (UI): Minimapa en tiempo real, menú de pausa, pantalla de muerte, inventario, hotbar y sistema básico de crafteo.
🎮 Controles
Tecla / Botón	Acción
W, A, S, D	Movimiento
Barra Espaciadora	Saltar (Tocar dos veces en Creativo para volar)
Shift	Agacharse (Sneak) / Bajar (en vuelo)
Ctrl	Correr (Sprint)
Mouse	Mirar alrededor
Click Izquierdo	Romper bloque (Minar) / Interactuar UI
Click Derecho	Colocar bloque
1 - 9	Seleccionar slot de la Hotbar
Rueda del ratón	Navegar por la Hotbar
E	Abrir/Cerrar Inventario y Crafteo
F11	Pantalla Completa
Esc	Menú de Pausa
+ / -	Aumentar / Reducir la distancia de renderizado
🏗️ Arquitectura del Código
El proyecto está diseñado para ser directo y autocontenido. Estos son los módulos principales:
`MineCPP.cpp`: El corazón del juego. Contiene el bucle principal, la creación de la ventana (Win32), la captura de eventos (teclado/mouse), la lógica de renderizado principal y el dibujado de la interfaz de usuario (HUD, menús).
`World.cpp` / `World.h`: Maneja el almacenamiento del mundo (Hash map de chunks). Se encarga de la lógica de colisiones, actualizaciones de bloques, física de fluidos y de construir la malla 3D (Mesh) de cada chunk.
`Atlas.h`: Un generador de texturas fascinante. En lugar de cargar archivos `.png`, construye un atlas de texturas (Pixel Art) usando funciones matemáticas y ruido al momento de arrancar el juego.
`Noise.h`: Implementación matemática ligera de generadores de ruido (Perlin, FBM, Ridged) usados tanto para moldear el terreno 3D como para pintar las texturas.
`shaders_gl2.h`: Un mini-cargador de extensiones de OpenGL. Evita la necesidad de usar dependencias como GLEW/GLAD, mapeando manualmente los punteros de funciones modernas de OpenGL en Windows.
`shaders_src.h`: Contiene el código fuente (en strings raw de C++) de los shaders GLSL para el agua y el mundo.
🛠️ Cómo Compilar (Build)
Dado que es un proyecto nativo de Windows, compilarlo es muy sencillo si tienes Visual Studio (o las MSVC Build Tools).
Crea un proyecto vacío de C++ en Visual Studio.
Añade todos los archivos fuente (`.cpp`) y cabeceras (`.h`) al proyecto.
Asegúrate de compilar como aplicación de Ventanas (No de consola, aunque el IDE lo maneja bien).
El código ya incluye las directivas `#pragma comment` necesarias, por lo que se vinculará automáticamente con las bibliotecas estándar de Windows:
`opengl32.lib`
`user32.lib`
`gdi32.lib`
¡Compila y juega!
📜 Créditos
Desarrollado por RataLife.
