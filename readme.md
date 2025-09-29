blibiotecas q tem q instalar no arduino Observação: instale as bibliotecas: TinyGPSPlus, Adafruit BMP3xx (ou BMP388 lib), AESLib (ou TinyAES), Base64 (Arduino).

platformio.ini para instalar a blibioteca do arduino 

Instale a extensão PlatformIO IDE no VS Code (ícone quadrado → pesquisar por PlatformIO IDE → instalar).

Ela já traz o pio embutido, mas você só consegue usar dentro do terminal do VS Code.

👉 Instalação global (para usar no PowerShell/CMD):

Instale o Python 3.8+ (se ainda não tiver).

No terminal:

passo 3 – Instalar bibliotecas

No terminal integrado do VS Code (na pasta do projeto), rode:

pio lib install


Isso baixa todas as libs listadas no lib_deps.
Quando você compilar (pio run ou Build no VS Code), o PlatformIO já baixa automaticamente se faltar algo.


blibioteca do python pip install flask pycryptodome flask_cors
