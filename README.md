# 🥁 **Real Sigma Drums**

🎶 **Plugin de bateria acústica CLAP**, pronto para uso em qualquer DAW compatível com CLAP.
Sons reais, multi-mic, round robin e dinâmica natural — tudo empacotado e pronto para tocar.

[![CLAP](https://img.shields.io/badge/CLAP-Plugin-blue?style=for-the-badge\&logo=data\:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAMAAAAoLQ9TAAAAflBMVEUAAAD///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////+u+j2SAAAAKXRSTlMAAQQGCxwqMUFJS2h0foKSpbnF1uDp7fL0/P3+/v7+/v7+/v7+/v6/fCjjAAAAfklEQVQY02MQBQYGBhZWNnZ2BgYGJmYOTm5iZGBgaGZqenp5WZgYGJhZWVlFZXV1dXW1lZWVtXV1dXV1dXW1tbW1tXW1tbW1tbW1tbW1tbW1tbW1tXV1dXV1dXV1dXW1tbW1tXV1dXV1dXV1dXW1tbW1tXV1dXV1dXV1dXV1dXW1tbW1tbW1tXV1dXV1dXV1fAAMumB55kHqI+AAAAAElFTkSuQmCC)](https://github.com/free-audio/clap) [![License](https://img.shields.io/badge/license-MIT-lightgrey?style=for-the-badge)](#)

<img width="100%" alt="Real Sigma Drums Screenshot" src="https://github.com/user-attachments/assets/31393da0-3377-4e9e-9ce0-e77d225986bc" />

- [x] Plugin CLAP
- [x] 15 canais multi-mic
- [x] Round robin
- [x] Samples empacotados em .pak
- [x] Choke groups (hi-hat)
- [x] Curva de velocity quadrática
- [x] Samples estéreo nos overheads/room
- [ ] Interface gráfica
- [ ] Windows & macOS builds

## [DOWNLOAD](https://github.com/realsigmamusic/realsigmadrums/releases/latest)

## 🧰 **Pacote**
```
realsigmadrums.clap/
├── plugin.clap
└── sounds.pak
```

## ⚙️ **Instalação (Linux)**
1. Dê permissão de execução ao instalador *(apenas na primeira vez)*:
```bash
chmod +x install.sh
```
2. Execute o instalador:
```bash
./install.sh
```
O plugin será instalado em:
```
~/.clap/realsigmadrums.clap/
```


| Note       | Instrument                 | Outputs                       |
|------------|----------------------------|-------------------------------|
| 35-36      | Kick                       | In, Out, OH L/R, Room L/R     |
| 37         | Sidestick                  | Top, Bottom, OH L/R, Room L/R |
| 38, 40     | Snare                      | Top, Bottom, OH L/R, Room L/R |
| 42, 44, 46 | Hi-Hat (closed/pedal/open) | Mic, OH L/R, Room L/R         |
| 50, 48, 47 | Rack Toms                  | Mic, OH L/R, Room L/R         |
| 45, 43, 41 | Floor Toms                 | Mic, OH L/R, Room L/R         |
| 49, 57     | Crashes                    | OH L/R, Room L/R              |
| 51, 53, 59 | Ride (bow/bell/edge)       | OH L/R, Room L/R              |
| 52, 55     | China, Splash              | OH L/R, Room L/R              |

## Aviso Importante

Se você carregar o plugin em modo estéreo simples, apenas o som do bumbo (kick) será reproduzido.
Para ouvir todos os instrumentos, abra o plugin em um host que suporte múltiplas saídas e ative as demais faixas de áudio do plugin.

## Output Channels
1. Kick In
2. Kick Out
3. Snare Top
4. Snare Bottom
5. Hihat
6. Racktom 1
7. Racktom 2
8. Racktom 3
9. Floortom 1
10. Floortom 2
11. Floortom 3
12. Overhead L
13. Overhead R
14. Room L
15. Room R