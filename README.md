# Estação/Controle – Ambiente de Dev (C++ + Python GUI)

Bem-vindo! Este repositório já vem pronto para você desenvolver em **C++ (CMake+Ninja)** e **Python (PyQt6)** dentro de um **Dev Container** (Docker + VS Code).
A ideia é padronizar o ambiente de todos os membros do grupo, evitando “funciona na minha máquina”.

## 1) Visão rápida do projeto

* **Linguagens:** C++ (núcleo de controle/concorrência/infra) e Python (GUI local com PyQt6, ferramentas de apoio).
* **Objetivo:** facilitar o desenvolvimento de aplicações de automação em tempo real com interface gráfica simples para testes locais.
* **Por que Dev Container?** Para que todos usem as mesmas versões de compiladores, bibliotecas e ferramentas – sem briga de dependências.

---

## 2) Pré-requisitos

1. **Docker** instalado e rodando.
2. **VS Code** com a extensão **Dev Containers** instalada.

---

## 3) Estrutura (arquivos principais)

``` text
.devcontainer/
  ├─ devcontainer.json        # Config do VS Code
  ├─ docker-compose.yml       # Orquestração do container
  └─ Dockerfile               # Imagem com toolchain C++ + Python GUI deps

.vscode/
  ├─ tasks.json               # Tasks para automação (build, run)
  └─ launch.json              # Configuração de debug

requirements.txt              # Dependências Python (PyQt6, etc.)
CMakeLists.txt                # Raiz do projeto C++
src/                          # Código-fonte C++
gui/                          # Código Python da interface
```

---

## 4) Como abrir no VS Code (Dev Container)

1. **Abra a pasta** do repositório no VS Code.
2. Pressione **F1 → Dev Containers: Reopen in Container**.
3. Aguarde a criação da imagem/contêiner.
4. O VS Code vai entrar automaticamente no ambiente com:

   * Compiladores C/C++, CMake, Ninja, gdb/lldb.
   * Python 3.12 + pip.
   * Bibliotecas de base para GUI PyQt6.
   * Extensões do VS Code já instaladas (Python, C++, CMake, Copilot, Git Graph etc.).
5. ***Small fix: sudo chown -R $(id -u):$(id -g) /home/atr/***

---

## 5) Comandos úteis

```bash
# Abrir/reattach no container (VS Code)
F1 → Dev Containers: Reopen in Container

# Reconstruir o container
F1 → Dev Containers: Rebuild and Reopen in Container

# Erro de permissão (dentro do container)
sudo chown -R $(id -u):$(id -g) /home/atr

# Compilar o projeto C++
CTRL + SHIFT + b  # Executa a task de build (CMake + Ninja)

# Rodar o programa C++ (após build)
F1 → Run Task → Run (Debug)

# Debug o programa C++ (após build)
CTRL + SHIFT + D → Iniciar Debug (F5)
```

---

**Pronto!** Se algo quebrar, copie o erro e mande no grupo – fica fácil ajustar porque todo mundo tem o **mesmo ambiente**.
