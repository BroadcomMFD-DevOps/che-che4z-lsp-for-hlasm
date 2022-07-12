const fs = require('fs');
const path = require('path');

const transfer = path.join(__dirname, "..", "..", "node_modules/basic-ftp/dist/transfer.js");
const content = fs.readFileSync(transfer).toString();
const new_content = content.replace(/source.pipe\(dataSocket\).once\("finish", \(\) => {.*?}\)/s, 'source.pipe(dataSocket.once("close", () => { resolver.onDataDone(task); }))');
fs.writeFileSync(transfer, new_content);