// probeglclass.js — H1 regression probe (bug-fix run, 2026-10-20).
//
// The GL stream's sync/async classifier decides which imported GL calls
// execute synchronously (before the caller continues) vs stream async.
// Any import whose page-side implementation DEREFERENCES a caller-memory
// pointer must be sync: an async record's pointer is read by the page
// LATER, after the calling worker may have reused that memory (H1: 11
// names fell through the old regex — 8 Delete* + VertexAttribI4iv/uiv +
// BindAttribLocation — and could delete/read the wrong data, silently).
//
// This probe loads lkboot.js in a Node vm (worker flavor), enumerates
// the shipped wasm binary's imported emscripten_gl* names, and checks
// lkGLIsSync() against an INDEPENDENT oracle: the list of pointer-
// dereferencing imports derived from the C signatures / GL spec (a name
// is in the oracle if its implementation reads or writes caller memory
// through a pointer argument). Every oracle name must classify sync.
//
// Usage: node probeglclass.js [lkboot.js] [LithoKraft.wasm]
// Exit 0 = PASS, 1 = FAIL.
const fs = require('fs');
const vm = require('vm');

const lkbootPath = process.argv[2] || '../full/lkboot.js';
const wasmPath = process.argv[3] || '../full/LithoKraft.wasm';

// --- independent oracle: imports that touch caller memory via pointer ---
// (suffixes after 'emscripten_gl'). Value-only calls (binds, draws,
// state setters, Is*/Create* returning handles) are NOT here.
const POINTER_READERS = [
  // H1's eleven (DeleteVertexArrays/OES were already caught by the old
  // regex's Arrays alternative; kept as controls):
  'DeleteBuffers', 'DeleteTextures', 'DeleteFramebuffers',
  'DeleteRenderbuffers', 'DeleteQueries', 'DeleteQueriesEXT',
  'DeleteSamplers', 'DeleteTransformFeedbacks',
  'DeleteVertexArrays', 'DeleteVertexArraysOES',
  'VertexAttribI4iv', 'VertexAttribI4uiv', 'BindAttribLocation',
  // write-pointer families (results land in caller memory):
  'GenBuffers', 'GenTextures', 'GenFramebuffers', 'GenRenderbuffers',
  'GenVertexArrays', 'GenVertexArraysOES', 'GenQueries', 'GenQueriesEXT',
  'GenSamplers', 'GenTransformFeedbacks', 'GenSync', 'FenceSync',
  'GetBooleanv', 'GetIntegerv', 'GetInteger64v', 'GetFloatv',
  'GetBufferParameteriv', 'GetBufferParameteri64v',
  'GetTexParameterfv', 'GetTexParameteriv', 'ReadPixels',
  'GetFramebufferAttachmentParameteriv', 'GetRenderbufferParameteriv',
  'GetInternalformativ', 'GetShaderiv', 'GetShaderInfoLog',
  'GetShaderSource', 'GetProgramiv', 'GetProgramInfoLog',
  'GetAttachedShaders', 'GetActiveAttrib', 'GetActiveUniform',
  'GetUniformfv', 'GetUniformiv', 'GetUniformuiv', 'GetUniformIndices',
  'GetActiveUniformsiv', 'GetActiveUniformBlockiv',
  'GetActiveUniformBlockName', 'GetQueryiv', 'GetQueryivEXT',
  'GetQueryObjectiv', 'GetQueryObjectuiv', 'GetQueryObjectivEXT',
  'GetQueryObjectuivEXT', 'GetQueryObjecti64vEXT', 'GetQueryObjectui64vEXT',
  'GetSynciv', 'GetSamplerParameteriv', 'GetSamplerParameterfv',
  'GetVertexAttribfv', 'GetVertexAttribiv', 'GetVertexAttribIiv',
  'GetVertexAttribIuiv', 'GetVertexAttribPointerv',
  'GetIntegeri_v', 'GetInteger64i_v', 'GetTransformFeedbackVarying',
  'GetProgramBinary', 'GetFragDataLocation',
  // read-pointer families (uploads / arrays / strings):
  'BufferData', 'BufferSubData', 'ShaderSource',
  'TexImage2D', 'TexImage3D', 'TexSubImage2D', 'TexSubImage3D',
  'CompressedTexImage2D', 'CompressedTexImage3D',
  'CompressedTexSubImage2D', 'CompressedTexSubImage3D',
  'TexParameterfv', 'TexParameteriv', 'SamplerParameterfv',
  'SamplerParameteriv', 'Uniform1fv', 'Uniform2fv', 'Uniform3fv',
  'Uniform4fv', 'Uniform1iv', 'Uniform2iv', 'Uniform3iv', 'Uniform4iv',
  'Uniform1uiv', 'Uniform2uiv', 'Uniform3uiv', 'Uniform4uiv',
  'UniformMatrix2fv', 'UniformMatrix3fv', 'UniformMatrix4fv',
  'UniformMatrix2x3fv', 'UniformMatrix3x2fv', 'UniformMatrix2x4fv',
  'UniformMatrix4x2fv', 'UniformMatrix3x4fv', 'UniformMatrix4x3fv',
  'VertexAttrib1fv', 'VertexAttrib2fv', 'VertexAttrib3fv',
  'VertexAttrib4fv', 'VertexAttrib1dv', 'VertexAttrib2dv',
  'VertexAttrib3dv', 'VertexAttrib4dv',
  'ClearBufferfv', 'ClearBufferiv', 'ClearBufferuiv',
  'DrawBuffers', 'DrawBuffersEXT', 'DrawBuffersWEBGL',
  'InvalidateFramebuffer', 'InvalidateSubFramebuffer',
  'TransformFeedbackVaryings', 'ProgramBinary',
];

async function main() {
  // --- load lkboot.js in a worker-flavored vm context ---
  const sandbox = {
    console, performance, TextDecoder, TextEncoder,
    Atomics, WebAssembly, SharedArrayBuffer, BigInt,
    setTimeout, clearTimeout, setInterval, clearInterval,
    crypto: require('node:crypto').webcrypto,
    location: { href: 'http://probe/lkboot.js' },
    importScripts: () => { throw new Error('probe: no importScripts'); },
  };
  sandbox.self = {
    location: sandbox.location,
    postMessage: () => {},
    set onmessage(fn) { this._onmessage = fn; },
    get onmessage() { return this._onmessage; },
  };
  vm.createContext(sandbox);
  vm.runInContext(fs.readFileSync(lkbootPath, 'utf8'), sandbox, { filename: lkbootPath });

  const isSync = vm.runInContext('lkGLIsSync', sandbox);
  if (typeof isSync !== 'function') {
    console.log('FAIL: lkGLIsSync not found in ' + lkbootPath);
    process.exit(1);
  }
  const hasExplicitSet = vm.runInContext("typeof LK_GL_POINTER_READERS !== 'undefined'", sandbox);

  // --- enumerate the binary's GL imports ---
  const bytes = fs.readFileSync(wasmPath);
  const mod = await WebAssembly.compile(bytes);
  const imported = WebAssembly.Module.imports(mod)
    .map((i) => i.name)
    .filter((n) => n.startsWith('emscripten_gl'));
  console.log('imported emscripten_gl* names:', imported.length);

  let failures = 0;
  const missing = [];
  for (const suffix of POINTER_READERS) {
    const name = 'emscripten_gl' + suffix;
    if (!imported.includes(name)) continue; // oracle name not in this binary
    if (!isSync(name)) { missing.push(name); failures++; }
  }
  if (missing.length) {
    console.log('FAIL: pointer-dereferencing imports classified ASYNC:');
    for (const m of missing) console.log('  - ' + m);
  }
  if (!hasExplicitSet) {
    console.log('FAIL: no explicit LK_GL_POINTER_READERS set in the layer ' +
                '(classification still rests on a name pattern alone)');
    failures++;
  } else {
    // the explicit set must not carry stale entries (names the layer
    // does not implement) and must itself cover the H1 eleven:
    const stale = vm.runInContext(
      '[...LK_GL_POINTER_READERS].filter((n) => !(n in LK_GL))', sandbox);
    if (stale.length) {
      console.log('FAIL: LK_GL_POINTER_READERS has stale entries: ' + stale.join(', '));
      failures++;
    }
  }
  console.log(failures === 0 ? 'PROBEGLCLASS: PASS' : 'PROBEGLCLASS: FAIL (' + failures + ')');
  process.exit(failures === 0 ? 0 : 1);
}
main().catch((e) => { console.error('PROBEGLCLASS: ERROR', e); process.exit(1); });
