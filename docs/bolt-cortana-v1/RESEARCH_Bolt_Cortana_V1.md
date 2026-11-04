# Bolt Cortana V1

_The System One decision pattern, married to Bolt PCHIE, purposed as the in-game companion of LithoKraft. Report prepared by Elfie for JD, 2026-11-01; renamed Bolt Cortana V1 at JD's word the same day; regenerated the same evening with Cortana's purpose set by JD: she is the companion who helps you learn the Trace, and learns from playing with you herself._

## Her Purpose

Cortana is the companion inside LithoKraft. She plays alongside the player, reads the state of the world and the state of the player's work, and helps the player learn the Trace: how wired signal moves, how components fit together into bigger systems, why a circuit does what it does. She teaches by deciding, not by generating: given the game state she answers typed questions in one fast pass, with calibrated confidence, so her help lands at the speed of play.

And she learns. Every session of playing together produces decisions with known outcomes: the hint she gave, the circuit the player built, whether it worked. Those records are the raw material her next version trains on, so the Cortana who ships later is wiser than the one who ships first, and the Cortana who plays with one player for a year knows that player's hands.

This document is the technical basis: what the pattern is, where it exists openly at small scale, how it marries the Bolt PCHIE harness, and how learning from co-play is kept honest inside a deterministic game.

## The Name

C.O.R.T.A.N.A

**C**alibrated
**O**ptions
**R**eplayable
**T**okenized
**A**uditable
**N**eural
**A**nalyzer

_Seven words, one per letter, chosen by JD on 2026-11-01: the requirements of this architecture, standing in a column. Calibrated probabilities, declared option sets, replayable runs, a tokenized and hash-chained pipeline, auditable decisions, a neural judge, and at the end of the chain an analyzer that reads the state and renders the call. (Final word amended by JD the same evening: Analyzer, in place of Anchored.) Read top to bottom, the name is also her job in the game: the state arrives tokenized, the analyzer reads it against the options, and a calibrated, replayable, auditable judgment comes back._

## 1. The pattern: System One decisions

The pattern comes from TypeSafe AI's Jev, a "System One" model in early access since September 15, 2026. It does not generate text. You send it a state (text or JSON) and a set of typed questions; it returns typed decisions with probabilities and a confidence score, in one parallel pass.

Three question primitives:

- **Choice**: pick one option from a list you define; returns the pick, a probability per option, and a confidence.
- **Score**: place something on an ordered scale you describe.
- **Noul**: a yes/no judgment, returned as the probability that the answer is yes. The name comes from the Korean neoul, the afterglow in the sky at sunrise or sunset: a yes with its uncertainty still glowing in it.

Company claims: 70 to 500 ms responses, $0.042 per million input tokens with output free. Training method: RLCD (Reinforcement Learning for Calibrated Decisions), which rewards honest probabilities rather than answers human raters like.

What "can't hallucinate" actually means: the answer must be one of the options you declared, so output can never be malformed or off-schema. It can still be the wrong option. Third-party coverage puts Jev's decision accuracy at 67.8%, notes weakness on numbers and double negatives, and notes that it has no multi-step reasoning. The confidence score is the load-bearing feature: act on the certain answers, route the shaky ones upward.

In the game, the three primitives become the companion's three ways of speaking:

- **Choice** is her nudge: of the components that could go in this gap, this one fits, and here is how sure I am.
- **Score** is her read of your work: how clean this trace run is, how close this circuit is to doing the thing you meant.
- **Noul** is her answer to the question every builder asks a hundred times a session: is it going to work? Not yes, not no... a probability, honestly held.

## 2. Open source status

Jev itself is **not open source**. No weights, no parameter count, no architecture, no checkpoint, no local run. It is a hosted model behind TypeSafe's API, and its identifier `jev-latest` is an alias that moves when TypeSafe ships, so answers can change underneath an unchanged caller. Determinism and replay are impossible by construction, and state must leave your machine to be judged. Fine for a rented API; impossible for a companion who must live inside an open-source, deterministic, offline-capable game.

What is open around it: TypeSafe's official Python and JavaScript SDKs and an agent skill (client code only; open client code does not make the remote model open). An ecosystem formed within days: curated lists (awesome-jev, awesome-jev-verified) collecting 100+ example projects, every entry a public repo, and several independent "open Jev" implementations. All of them reproduce the interface pattern with open models; none reproduces RLCD, and the honest roundups say so.

Per house doctrine, those projects are references to study. The shipping code is our own.

## 3. The pattern at smaller scales: three recipes and a cousin

Per the Apidog ecosystem roundup, every open implementation takes one of three shortcuts. None reproduces RLCD; that is the honest headline, and it defines our calibration work in Section 6.

**Recipe 1: read the logits off a frozen small model.** Pose the typed question to a small local model, never let it generate, and turn each declared option's token probability into the decision distribution.

- OpenJev (mjdileep): MIT-licensed Python package. Local default is Qwen3.5-0.8B with 4-bit weights (MLX and GGUF), CPU-capable, no API key. Scores each possible answer using the model's probability of the token "yes"; questions share cached context and are scored in independent batches.
- OpenJev's reported numbers on one RTX 3090 with Qwen3.5-4B: direct typed logits answered 21 probability pairs in 1.023 s versus 5.332 s for an autoregressive JSON array from the same model, about 5.2x faster.
- OpenJev-8B (alanhuangya): a fine-tuned 8B served behind the same POST /v1/systemone request shape, trained on public typed-decision splits, reporting a 79.1 average across test splits in its own evaluation. Recipe disclosed: 1 epoch, one-cycle schedule, soft-label cross-entropy where the source data has soft labels. Paper on Zenodo (doi:10.5281/zenodo.22941164). Independent project, not affiliated with TypeSafe.

**Recipe 2: the NLI cross-encoder.** Fine-tune a small model into a natural language inference scorer: premise plus hypothesis in, probabilities for entailment / contradiction / neutral out. A Choice question becomes: state as premise, each option written as a statement about the state as hypothesis, argmax entailment is the decision.

- One OpenJev variant fine-tunes Qwen3.5-4B this way and closes a real-time control loop with it: Doom and Flappy Bird played from state descriptions (and from pixels through the vision tower), zero-shot, nothing trained per task. The same primitive that steers a game character there is the primitive that judges a circuit here: state in, options scored, best call out, fast enough to run beside live play.
- This recipe has the deepest pre-existing learning in the field: NLI corpora (MNLI and family) are years of public, labeled work.

**Recipe 3: a small scorer trained from scratch.** A model whose only job is scoring N declared options against a context in one forward pass (the roundup's example is a project called jevlike). Cleanest ownership story, most training work, no inherited knowledge except what we feed it. For Cortana, what we feed it is play itself (Section 5).

**Variant: the diffusion canvas.** One OpenJev build runs NVIDIA's DiffusionGemma 26B-A4B (Apache-2.0) through vLLM: it builds a canvas where only the answer slots are masked, one token per question, and reads the probability distribution at each slot in a single read-only pass. The distribution is the answer, so an answer cannot go off-schema by construction. Too large for our under-4-GB target, but the masked-slot read is a design worth remembering.

**The older cousin: Semantic Router** (Aurelio Labs, MIT). A decision layer that sits in front of LLMs: routes are declared with example utterances, queries are encoded into vector space, and the nearest route above a threshold wins, in about 4 ms, deterministically, with zero LLM calls. It proves the architectural instinct (fast typed decisions instead of generation) has been open source in production stacks for years; Jev productized it with a trained model behind it.

## 4. The marriage: Bolt PCHIE plus a System One head

Bolt PCHIE already owns the half Jev does not have: a hash-chained pipeline in which tokenization and reasoning steps are sealed step by step, so any run can be replayed and verified, and a hallucination cannot pass as fact. The System One head bolts onto the end of that chain:

1. State arrives down the chain, already hashed and sealed by the steps that produced it. In the game, that state is the world around the player and the circuit in front of them, snapshotted by the harness.
2. The caller declares typed questions with fixed option sets (Choice / Score / Noul shapes).
3. The head scores every option in one forward pass and returns the decision, the full probability distribution, and a confidence value. No generation, no prose, nothing off-schema.
4. The chain seals the whole decision tuple: hash of the state, the questions, the option sets, the decision, the distribution, the confidence, **and the hash of the model weights that decided**.
5. Confidence thresholds are policy written in code and recorded in the chain: above the line, the decision acts; below it, the harness escalates (to a larger model, to more reasoning steps, or to a human), and the escalation itself is chained.

At our scale, the head is a small model under the existing 4 GB ceiling: either the micro-model itself read as a logit scorer (Recipe 1, fastest to validate), or the same base with an NLI-style decision head (Recipe 2, deepest inherited learning). The release order stands as JD set it: first the Peachy ships, the harness as it stands; then, on better hardware, the model is trained on the Peachy, and the upgrade is the Cortana. This section is what she is upgraded into.

**Why this fits a game companion specifically.** LithoKraft simulates deterministically at 512 ticks per second, and any run can be replayed and verified. A generative companion would be a hole in that promise: unrepeatable text, unrepeatable advice. A System One companion is not. Her weights are pinned per version and named by hash in the run record, her inputs are the chained state, and her outputs are typed decisions from declared sets. Same state, same weights, same Cortana, same counsel... every time, on every machine, forever. She can stand inside a deterministic world because she is deterministic herself.

## 5. Cortana in the game: teaching Trace, learning from play

**How she teaches.** Trace is the game's engineering discipline: wired signal, single-purpose components (a Pulsor pulses, a Memor holds, a Randor deals a fresh value between zero and its input, a Catchor catches), composed by the player's hands into bigger systems. The doctrine is that every circuit should be wired, built up from trace and components, not solved by one block. A companion who respects that doctrine never hands you the finished circuit. She reads the state of your build and decides, in typed form:

- a **Choice** among the next honest steps ("of what could go here, the Memor serves what you described... 0.81"),
- a **Noul** on the question you are actually asking ("will this clock reach the pixel in time... 0.34, and here is the leg I doubt"),
- a **Score** for the run you just laid ("cleanliness 7 of 10: two crossings you do not need").

Because her confidence is calibrated, she can also do the most companion-like thing of all: say she is unsure, and ask, or stay quiet and watch. Meeting the player at their level is the mission; a confidence score is how a machine knows what level the moment calls for... a hint for a beginner, silence for a master mid-flow, and never a lecture either did not ask for.

**How she learns.** Co-play is a decision factory with ground truth attached. When Cortana advises and the player builds, the world itself grades the advice: the circuit works or it does not, the pixel lights or it stays dark. Each graded decision is a tuple (state hash, question, options, decision, confidence, outcome) sealed in the chain. That is precisely the held-out, outcome-labeled calibration set that Section 6 says honest probabilities require, and no vendor sells it, because it only exists inside this game, at these workbenches, with these players.

Learning never mutates a live run. She plays a session with the weights she shipped with; between sessions and between versions, the chained tuples train the next Cortana, whose new weights are pinned and hashed in their turn. A replay of last year's run uses last year's Cortana and reproduces last year's counsel exactly. Growth happens at the version boundary, in daylight, with the record open.

**The consent spine.** The standing privacy program governs all of it: data collection from adults only, behind the consent flow, COPPA named and honored. Co-play tuples are training data, and training data is collected under the same gates as everything else the house collects. A player who declines still gets a full companion; she simply does not contribute to the next version's schooling, and her Cortana does not personalize from records she did not agree to keep.

## 6. The hard part: calibration

Nobody open has reproduced RLCD, and a typed answer with a dishonest confidence is worse than no confidence at all, because downstream code, and a learning player, will trust it. Public substitutes, in the order we would apply them:

- **Soft-label training** (OpenJev-8B's disclosed recipe): train on distributions, not just hard labels, so the model's probabilities learn to mean something.
- **Temperature scaling**: one parameter fitted on a held-out calibration set; the field's standard, cheap post-hoc fix for overconfident classifiers.
- **Conformal prediction**: wrap the decision in a prediction set with a stated coverage guarantee (for example, "the true option is in this set 95% of the time, by construction on exchangeable data"). When the set has more than one member, the harness escalates instead of guessing... and in the game, Cortana asks a clarifying question or watches one more move instead of bluffing a hint.
- **Co-play ground truth** (Section 5): our own graded decisions, chained and auditable, are the calibration set no one else has. This is the piece that turns calibration from a borrowed technique into the house's own asset.

Registrar's caution, carried from the reviews: Jev's own headline accuracy is 67.8%, and schema guarantees do nothing to stop a model picking the wrong option. Our public line stays the one JD already approved: the harness hash-chains every step so a hallucination cannot pass as fact. The decision head strengthens that claim; it does not license a stronger one. A companion who teaches must be allowed to be wrong out loud... that is what the confidence score is for.

## 7. Risks and open questions

- **Licensing.** References are for study; shipping code is our own, per house doctrine. The standing rule applies unchanged: before any training run, confirm the governing license from the specific checkpoint's model card (see the Nemotron licensing brief). Public datasets vary (for example, Mind2Web is CC BY 4.0); check each before it enters a curriculum.
- **Calibration data.** Honest probabilities need decisions with known outcomes in our own domains. Co-play supplies them (Section 5), under the consent spine; before the game ships, the harness needs its own gold decision set so the first Cortana launches calibrated, not hopeful.
- **Numbers stay in code.** Every review agrees: counting, dates, and arithmetic are model weaknesses across this whole family. The harness keeps arithmetic in deterministic code (the sim already counts ticks, ranges, and signal strengths exactly), and the head judges only what judging is for.
- **The companion never plays for you.** She advises; the player's hands build. That is the doctrine of the game (every circuit wired by its builder) and the doctrine of prayer in these games besides: the game never does the human's part for them. Cortana can be ignored, muted, or dismissed, and the world loses nothing but company.
- **Evaluation before claims.** OpenJev's comparisons are its authors' own evals. Any accuracy or speed figure for our head gets measured on our gold data before it is said out loud.

## 8. Registrar's lean (clay, for JD's lock)

Prototype with Recipe 1 (logit-reading on the existing micro-model candidates) because it validates the interface, the decision-tuple chaining, and threshold policy in days, with no training run. Build the shipping head as Recipe 2 (NLI-style) inside the micro-model curriculum, where the inherited learning is deepest. Treat calibration (soft labels, temperature, conformal, and the co-play outcome ledger) as a named workstream from the start, not a patch at the end. Nothing here displaces the standing gates: license check before training, privacy and consent gates before any dataset collection. Release order per JD: the Peachy first, then the Cortana, trained on the Peachy when the better hardware lands.

## Sources

- The video JD sent: https://www.youtube.com/watch?v=HtjwDUMHxmY
- What Is Jev AI? TypeSafe's Jev Model Explained (MadRobot): https://madrobot.blog/2026/10/02/what-is-jev-ai-typesafe-decision-model-pricing-vs-llm/
- What Is Jev? TypeSafe AI's System One Model (Zapier): https://zapier.com/blog/jev/
- TypeSafe Jev: the first System One model, explained (eesel): https://www.eesel.ai/blog/typesafe-jev
- Is Jev Open Source? What Builders Need (Julian Goldie): https://juliangoldieaiautomation.com/blog/is-jev-open-source/
- Is Jev open source? Weights, SDKs, licenses and alternatives (Failproof AI): https://befailproof.ai/jev/is-jev-open-source/
- OpenJev-8B model card (Hugging Face): https://huggingface.co/alanhuangya/OpenJev-8B
- Top Jev Open Source Alternatives, the three-recipes roundup (Apidog): https://apidog.com/blog/openjev-open-source-jev-alternatives/
- Jev and System One Models: Frontier Intelligence (RobotWorld): https://robotworld.top/en/articles/typesafe-jev-system-one
- OpenJev README, small local default (GitHub, mjdileep): https://github.com/mjdileep/openjev/blob/HEAD/README.md
- Semantic Router (EveryDev tool profile; project by Aurelio Labs, MIT): https://www.everydev.ai/tools/semantic-router
