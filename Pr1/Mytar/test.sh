#!/bin/bash

# STATUS:
# 0 - No errors
# 1 - Program not working as specified
# 2 - Program not found

# 1. Comprobamos que el programa mytar está en el directorio actual y que es
# ejecutable. En caso contrario mostramos un mensaje informativo por pantalla
if [ ! -x ./mytar ]; then
    echo "El ejecutable mytar no existe">&2
    exit 2
fi

# 2. Comprobamos si existe un directorio tmp dentro del directorio actual.
# Si existe lo borramos, incluyendo todo lo que haya dentro de el
[ -d tmp ] && rm -r tmp

# 3. Creamos un nuevo directorio temporal tmp dentro del directorio actual
# y cambiamos a este directorio
mkdir tmp && cd tmp

# 4. Creamos 3 ficheros dentro del directorio

# file1.txt con el contenido "Hello World!"
echo "Hello World!" > file1.txt
# file2.txt con una copia de las 10 primeras lineas de /etc/passwd
head -n10 /etc/passwd > file2.txt
# file3.dat con un contenido aleatorio binario de 1024 bytes, tomado del
# /dev/urandom
head -c1024 /dev/urandom > file3.dat

filearray=(file1.txt file2.txt file3.dat)

# 5. Invocamos mytar
../mytar -cf filetar.mtar ${filearray[@]} || { echo "Error while creating mytar" >&2; exit 1; }
[ -f filetar.mtar ] || { echo "El fichero filetar.mtar no se ha creado"; exit 1; }

# 6 Creamos un directorio out/tmp y copiamos mtar al nuevo directorio
mkdir out || { echo "Failed to create dir out"; exit 1; }
cp filetar.mtar out/

# 7. Cambiamos al directorio out y extraemos el contenido del tarball
cd out
../../mytar -xf filetar.mtar || { echo "Error while extracting mytar" >&2; exit 1; }

# 8. Usamos diff para comparar los ficheros
for i in "${filearray[@]}"; do
    echo $i
    diff "../$i" "$i" || exit 1
done;

# 9. Si los tres ficheros son originales, mostramos "Correct" por pantalla y
# retornamos 0. Sy hay algun error devolvemos 1
echo "Correct"
exit 0
