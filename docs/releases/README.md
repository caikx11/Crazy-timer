# Como publicar um release (GitHub)

Passo a passo para quem mantém o projeto. O release oferece um único arquivo, `crazy-time.wh.cpp`, que o usuário cola no editor do Windhawk.

1. Leve o trabalho para a `main` (PR e merge) e atualize o clone: `git checkout main && git pull`.
2. Confira a versão em `src/windows/mod_entry.cpp` (`@version`) e atualize `docs/releases/vX.Y.Z.md`.
3. Rode os testes: `./scripts/test.ps1` e `./scripts/test-executor.ps1`.
4. Gere o arquivo: `./scripts/package-release.ps1`. Ele cria `dist/crazy-time.wh.cpp` e `dist/crazy-time.wh.cpp.sha256`.
5. Marque e envie a tag (troque pela versão): `git tag v0.6.0` e `git push origin v0.6.0`.
6. No GitHub: **Releases → Draft a new release**, escolha a tag, use o texto de `docs/releases/vX.Y.Z.md`, anexe os dois arquivos de `dist/` e marque **Set as the latest release** (ou *pre-release* enquanto o Windows 10 não for verificado).

Este processo não publica no catálogo do Windhawk.
