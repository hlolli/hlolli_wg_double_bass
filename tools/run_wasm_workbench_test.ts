#!/usr/bin/env bun
/** Compile and play the prepared double_bass source with the browser workbench. */

import { pathToFileURL } from "node:url"
import { resolve } from "node:path"

interface Options {
  compilerRoot: string
  source: string
  csd: string
  output?: string
  pcmOutput?: string
  maxBlocks: number
}

function parseOptions(): Options {
  const values = process.argv.slice(2)
  const result: Partial<Options> = { maxBlocks: 10_000 }
  for (let index = 0; index < values.length; index += 2) {
    const name = values[index]
    const value = values[index + 1]
    if (!name?.startsWith("--") || value === undefined) {
      throw new Error("expected --name value arguments")
    }
    if (name === "--compiler-root") result.compilerRoot = value
    else if (name === "--source") result.source = value
    else if (name === "--csd") result.csd = value
    else if (name === "--output") result.output = value
    else if (name === "--pcm-output") result.pcmOutput = value
    else if (name === "--max-blocks") result.maxBlocks = Number(value)
    else throw new Error(`unknown argument: ${name}`)
  }
  if (!result.compilerRoot || !result.source || !result.csd) {
    throw new Error("--compiler-root, --source, and --csd are required")
  }
  if (typeof result.maxBlocks !== "number" ||
      !Number.isSafeInteger(result.maxBlocks) || result.maxBlocks < 1) {
    throw new Error("--max-blocks must be a positive safe integer")
  }
  return result as Options
}

async function importFrom(path: string): Promise<any> {
  return import(pathToFileURL(path).href)
}

async function browserFactory(compilerRoot: string): Promise<any> {
  const entry = resolve(
    compilerRoot,
    "node_modules/@csound/browser/dist/csound.js"
  )
  const source = await Bun.file(entry).text()
  const expected =
    "const Csound = kd; const libcsound = __lcs__; export { Csound, libcsound }; export default Csound;"
  if (!source.includes(expected)) {
    throw new Error("the pinned Csound browser entry changed")
  }
  return new Function(source.replace(expected, "return __lcs__;"))()
}

async function main(): Promise<void> {
  const options = parseOptions()
  const compilerRoot = resolve(options.compilerRoot)
  const source = await Bun.file(options.source).text()
  const csd = await Bun.file(options.csd).text()
  const compileModule = await importFrom(
    resolve(compilerRoot, "src/compiler/compile.ts")
  )
  const archiveModule = await importFrom(
    resolve(compilerRoot, "src/compiler/sdk-archive.ts")
  )
  const archive = new Uint8Array(
    await Bun.file(
      resolve(
        compilerRoot,
        "node_modules/@csound/wasm-bin/lib/csound-plugin-sdk.tar.gz"
      )
    ).arrayBuffer()
  )
  const headers = archiveModule.extractCsoundHeaders(Bun.gunzipSync(archive))
  await compileModule.initializeCompiler(() => {})
  const result = await compileModule.compilePlugin(source, "c", headers)
  if (!result.ok || !(result.wasm instanceof ArrayBuffer)) {
    const details = result.diagnostics
      ?.map((row: { line: number; message: string }) =>
        `${row.line}: ${row.message}`
      )
      .join("\n")
    throw new Error(details || result.message || "browser compile failed")
  }
  const wasm = result.wasm
  const module = await WebAssembly.compile(wasm)
  const exports = new Set(
    WebAssembly.Module.exports(module).map((entry) => entry.name)
  )
  if (!exports.has("__wasm_call_ctors") || !exports.has("csound_opcode_init")) {
    throw new Error("browser plugin lacks required exports")
  }

  Object.defineProperty(globalThis, "window", {
    value: {
      atob: globalThis.atob.bind(globalThis),
      btoa: globalThis.btoa.bind(globalThis),
      webkitAudioContext: undefined
    },
    configurable: true
  })
  let blocks = 0
  let channels = 0
  let sampleRate = 0
  let pcmPeak = 0
  const pcm: number[] = []
  try {
    const factory = await browserFactory(compilerRoot)
    const api = await factory({ withPlugins: [wasm] })
    const csound = api.csoundCreate()
    try {
      if (api.csoundSetOption(csound, "-n") !== 0 ||
          api.csoundSetOption(csound, "--sample-accurate") !== 0 ||
          api.csoundSetOption(csound, "--num-threads=1") !== 0) {
        throw new Error("browser Csound rejected render options")
      }
      if (api.csoundCompileCSD(csound, csd) !== 0) {
        throw new Error("browser Csound rejected the smoke CSD")
      }
      if (api.csoundStart(csound) !== 0) {
        throw new Error("browser Csound did not start")
      }
      channels = api.csoundGetNchnls(csound)
      sampleRate = api.csoundGetSr(csound)
      const ksmps = api.csoundGetKsmps(csound)
      const zeroDbfs = api.csoundGet0dBFS(csound)
      if (api.csoundGetSizeOfMYFLT() !== 8 || channels < 1 ||
          sampleRate <= 0 || ksmps < 1 || zeroDbfs <= 0) {
        throw new Error("browser Csound returned invalid audio facts")
      }
      let status = 0
      while (status === 0 && blocks < options.maxBlocks) {
        status = api.csoundPerformKsmps(csound)
        blocks += 1
        if (status === 0 && options.pcmOutput) {
          const spout = new Float64Array(
            api.getMemory().buffer,
            api.csoundGetSpout(csound),
            ksmps * channels
          )
          for (const rawSample of spout) {
            const sample = rawSample / zeroDbfs
            if (!Number.isFinite(sample)) {
              throw new Error("browser Csound produced non-finite PCM")
            }
            pcm.push(sample)
            pcmPeak = Math.max(pcmPeak, Math.abs(sample))
          }
        }
      }
      if (status === 0) throw new Error("browser score did not end")
    } finally {
      api.csoundDestroy(csound)
    }
  } finally {
    Reflect.deleteProperty(globalThis, "window")
  }

  const facts = {
    schema_version: 1,
    source_bytes: new TextEncoder().encode(source).byteLength,
    wasm_bytes: wasm.byteLength,
    performed_blocks: blocks,
    pcm_channels: channels,
    pcm_sample_rate: sampleRate,
    pcm_samples: pcm.length,
    pcm_peak: pcmPeak
  }
  if (options.pcmOutput) {
    if (pcm.length === 0) throw new Error("browser Csound returned no PCM")
    const bytes = new ArrayBuffer(pcm.length * 8)
    const view = new DataView(bytes)
    pcm.forEach((sample, index) => view.setFloat64(index * 8, sample, true))
    await Bun.write(options.pcmOutput, bytes)
  }
  if (options.output) {
    await Bun.write(options.output, `${JSON.stringify(facts, null, 2)}\n`)
  }
  console.log(
    `PASS WASM: source=${facts.source_bytes} wasm=${facts.wasm_bytes} ` +
      `blocks=${facts.performed_blocks} pcm=${facts.pcm_samples}`
  )
}

await main().catch((error) => {
  console.error(`WASM workbench test failed: ${error}`)
  process.exit(1)
})
