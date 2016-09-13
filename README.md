# tp-2016-2c-CodeTogether

// LF - 2016.09.06
Pasos para crear el FS
- Cree el archivo Osada.bin
- Le di el tamaño con la instruccion: truncate --size 100M Osada.bin
- Lo formatee con : ./osada-format ../tp-2016-2c-CodeTogether/Osada.bin


# Intercambio de mensajes

***********************
MAPA - ENTRENADOR
***********************
Entrenador -> Mapa | entrarAlMapa(char entrenadorID)

Mapa -> Entrenador | turnoConcedido()

Entrenador -> Mapa | t_mapa_pos obtenerUbicacionPokenest(char pokemonID)

Entrenador -> Mapa | movermeA(t_mapa_pos nuevaPos)

Entrenador -> Mapa | capturarPokemon(char pokemonID)
Mapa -> Entrenador | confirmarCaptura(metadataPokemon)

Entrenador -> Mapa | meMori()

Entrenador -> Mapa | cumpliMisObjetivos()
Mapa -> Entrenador | entregarMedalla()

//TODO: Batallas, Muerte de entrenador y Vidas
