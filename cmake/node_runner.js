#!/usr/bin/env node
import(require('url').pathToFileURL(require('path').join(__dirname, '${INPUT_FILE}'))).then(m => m.default({ arguments: process.argv.slice(2) }));