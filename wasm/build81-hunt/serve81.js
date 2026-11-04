const http = require('http'), fs = require('fs'), path = require('path');
const root = process.argv[2], port = +process.argv[3];
const mime = {'.html':'text/html','.js':'text/javascript','.wasm':'application/wasm','.data':'application/octet-stream','.md':'text/plain'};
http.createServer((req,res)=>{
  let p = decodeURIComponent(req.url.split('?')[0]); if (p==='/') p='/index.html';
  const f = path.join(root,p);
  fs.readFile(f,(e,d)=>{ if(e){res.writeHead(404);res.end();return;}
    res.writeHead(200,{'Content-Type':mime[path.extname(f)]||'application/octet-stream','Cross-Origin-Opener-Policy':'same-origin','Cross-Origin-Embedder-Policy':'require-corp','Cross-Origin-Resource-Policy':'same-origin'});
    res.end(d); });
}).listen(port,'127.0.0.1',()=>console.log('serving',root,'on',port));
