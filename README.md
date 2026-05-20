Hola buenos dias/tarde, en especial a Samuel q prob es la unica persona (siacaso) que lea este README.
Para entender mejor el codigo y la logica detras de el organice los comentarios en los archivos internos por orden de importancia. 

# Orden de los Documentarios
* 1. Inlcude: basta con leer el README para entenderlo, sin emebargo el archivo de configuracion tiene mas informacion detallada en caso de que se quiera modificar directamente la configuracion del proyecto base. 

* 2. lib:
    * 1. ProfileManager.h: Lo configuracion logica real, se encuentran cosas importantes como el Nivel de Sensibilidad (en el codigo ta en inglish pero es pq suena mas cool), el sistema de "cooldown" (Es lit eso, lo q estas pensando, funciona igual q en los juegos no hay q explicar mucho, sin embargo, las metricas de prueba/debug y funcionales estan preestablecidas asiq toca configurarlas dependiendo el entorno de prueba requerido o la modificacion precisa que se le quiera hacer al sistema [Leer la documentacion correspondiente]), el manejo de histeria, etc.
    Sin embargo, se explica de forma introductoria y no entra en manejo de logica literal, sirve pa entender que estaba pensando yo cuando defini los niveles de sensibilidad y para entender como actua el sistema. 
    * 2. Actors: Todo lo que se encuentre en la carpeta, recomiendo empezar por el README obvio, seguido de WaterActor y HeatActor, aqui se explica que se hace con las señales de los sensores.
    Cabe recalcal que algunas cosas en HeatActor no funcionan de la manera exacta que queria pq el foco ya no se maneja de forma automatica por diversos motivos, entonces, ahora funciona como clase de tipo advisor. Es mas que nada para saber el estado del foco desde el sistema.
    * 3. ProfileManager.cpp: Ahora si se explica completo y no como introduccion, ahora si explica la logica detallada, considero que es mejor saber como se reciben los datos y como se procesan antes de entender los ciclos del sistema.
    * 4. Sensor: Lo unico relevante quizas sea la clase base (Sensor) q como se explica es pa añadir o modificar sensores extras sin dañar el funcionamiento base, lo pongo de ultimo pq de plano no lo documente, dde vaina tu leer el header del sensor de temperatura y ya entiendes todo, no tiene tanto mistisismo, aparte pq esto es pa leer los datos de su respectivo sensor. 
En la parte mas primitiva del software esta simplemente leer los datos y reaccionar a ellos, este sistema se encarga de automatizar la reaccion e informar su estado cuando sea requerido. 
* 3. src: Aqui pues solo esta el main, ya el loop del sistema como tal, es el mas largo y por ende de ultimo, no es menos importante, de hecho tiene bastante logica y flujo pero a quien va a querer leer tanto, like wtf.

# Dicho eso deberia emplear el MQTT :(