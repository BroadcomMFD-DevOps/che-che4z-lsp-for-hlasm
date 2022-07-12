const fs = require('fs');
const path = require('path');

const base = path.join(__dirname, "..", "..");

const transfer = path.join(base, "node_modules/basic-ftp/dist/transfer.js");
const content = fs.readFileSync(transfer).toString();
const new_content = content.replace(/source.pipe\(dataSocket\).once\("finish", \(\) => {.*?}\)/s, 'source.pipe(dataSocket.once("close", () => { resolver.onDataDone(task); }))');
fs.writeFileSync(transfer, new_content);

fs.mkdirSync(path.join(base, "bin"), { recursive: true });
fs.copyFileSync(path.join(base, "terse", "terse.js"), path.join(base, "bin", "terse.js"));
fs.copyFileSync(path.join(base, "terse", "terse.wasm"), path.join(base, "bin", "terse.wasm"));