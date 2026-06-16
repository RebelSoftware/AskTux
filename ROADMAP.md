## Priority-ordered improvements

---

### Phase 1 — Quick UX wins (low effort, high impact)

* [Completed] **Model pull optimisation** — Currently OllamaClient always calls `/api/pull` even when the model is already available, adding 1–3s of latency. Change to: `GET /api/tags` first, only `POST /api/pull` if the model is absent. [Completed]

* **Improve status messages** — Hide noisy internal progress ("pulling manifest", layer digests). Show a simple "Working…" label or a subtle animated indicator. Keep detailed progress in a debug log.

---

### Phase 2 — Smarter conversations

* **Chat history for follow-up discussion** — Keep a rolling window of recent Q&A pairs (target ~4k tokens). Prepend to the system prompt so the LLM has conversation context. Prune oldest turns when budget is exceeded.

* **Read-only tool use** — Give the LLM the ability to query local resources and incorporate the results into its answer.
    * *Local man pages* — Use `man -w <topic>` to locate the file, read it, and feed relevant sections as context.
    * *Installed packages* — Query `dpkg-query` or parse dpkg status and include package list in system context.

---

### Phase 3 — Flexibility

* **OpenAI-compatible API support** — `OpenAIClient` already exists. Add a custom base URL field in Settings so users can point to any OpenAI-compatible endpoint (e.g. LM Studio, vLLM, Groq, etc.).

---

### Phase 4 — Distribution

* **Bind to f1 key**  — start application if the user presses the default help key

* **Installation packages for distros** — Build .deb / .rpm / AppImage. On install, offer to register the app as the F1-key default help viewer.

---

### Extra polish ideas

* **Syntax highlighting in code blocks** — Bundle highlight.js in `data/` and load it from the HTML template. Zero extra dependencies, big visual improvement.

* **Token batching for smoother streaming** — Instead of re-rendering the WebView on every single token, batch incoming tokens and refresh every ~100ms. Reduces flicker on fast responses.

* **Copy button on code blocks** — Small JS click handler injected into the HTML template. Copies just that code block to clipboard without selecting the whole page.

* **Keyboard shortcuts** — Ctrl+Enter to submit, Escape to cancel, Ctrl+L to clear output. Low code, big UX bump.

* **Typing indicator** — During streaming, append a blinking `▊` cursor at the end of the HTML so the user knows generation is still in progress.

* **Window size persistence** — Save/restore window geometry via `Config` so the window opens at the same size and position as last time.
