# Real Sigma Drums

Instrumento virtual CLAP de bateria acústica pronta para usar em qualquer DAW com suporte a CLAP.
[x] Plugin CLAP
[x] 15 canais multi-mic
[x] Round robin
[x] Samples empacotados em .pak
[x] Choke groups (hi-hat)
[x] Curva de velocity quadrática
[x] Samples estéreo nos overheads/room
[ ] Velocity layers
[ ] Interface gráfica

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
- Execute *dois cliques ou `./install.sh`* o arquivo `install.sh`.
O plugin será instalado em: `~/.clap/realsigmadrums.clap/`


## MIDI Map
| Note MIDI | Articulação  |
| --------- | ------------ |
| 35        | kick         |
| 36        | kick         |
| 37        | sidestick    |
| 38        | snare        |
| 40        | snare        |
| 41        | floortom 3   |
| 42        | hihat closed |
| 43        | floortom 2   |
| 44        | hihat pedal  |
| 45        | floortom 1   |
| 46        | hihat open   |
| 47        | racktom 3    |
| 48        | racktom 2    |
| 49        | crash 1      |
| 50        | racktom 1    |
| 51        | ride         |
| 52        | china        |
| 53        | ride bell    |
| 55        | splash       |
| 57        | crash 2      |
| 59        | ride edge    |

## Outputs 
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