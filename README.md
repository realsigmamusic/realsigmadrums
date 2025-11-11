# Real Sigma Drums

Instrumento virtual CLAP de bateria acústica pronta para usar em qualquer DAW com suporte a CLAP.

[![CLAP](https://img.shields.io/badge/CLAP-Plugin-blue)](https://github.com/free-audio/clap)

<img width="1366" height="723" alt="image" src="https://github.com/user-attachments/assets/31393da0-3377-4e9e-9ce0-e77d225986bc" />

- [x] Plugin CLAP
- [x] 15 canais multi-mic
- [x] Round robin
- [x] Samples empacotados em .pak
- [x] Choke groups (hi-hat)
- [x] Curva de velocity quadrática
- [x] Samples estéreo nos overheads/room
- [ ] Velocity Layers
- [ ] Windows & macOS builds
- [ ] Interface gráfica

## [DOWNLOAD](https://github.com/realsigmamusic/realsigmadrums/releases/latest)

## Conteúdo do pacote
```
RealSigmaDrums.zip/
├── install.sh
└── realsigmadrums.clap/
    ├── plugin.clap
    └── sounds.pak
```

## Instalação
- Dê permissão de execução ao instalador *(apenas na primeira vez)*:
```bash
chmod +x install.sh
```
- Execute *(dois cliques ou* `./install.sh` *no terminal)* o arquivo `install.sh`.
O plugin será instalado em: `~/.clap/realsigmadrums.clap/`

## Aviso Importante

Se você carregar o plugin em modo estéreo simples, apenas o som do bumbo (kick) será reproduzido.
Para ouvir todos os instrumentos, abra o plugin em um host que suporte múltiplas saídas e ative as demais faixas de áudio do plugin.

## Output Channels

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