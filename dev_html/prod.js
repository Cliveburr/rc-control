const minify = require('html-minifier').minify;
const fs = require('fs');
const path = require('path');
const htmlInlineExternal = require('html-inline-external')

console.log('Running prod...');

//const index = fs.readFileSync('index.html', 'utf8');
const prod_path = path.resolve('../main/wwwroot/index.html');

htmlInlineExternal({src: 'index.html'})
    .then(output => {

        // https://www.npmjs.com/package/html-minifier
        const index_min = minify(output, {
            collapseWhitespace: true,
            minifyCSS: true,
            minifyJS: true,
            removeAttributeQuotes: true,
            removeComments: true,
            sortClassName: true
        });

        fs.writeFileSync(prod_path, index_min, 'utf8');

        console.log("Bytes: " + index_min.length)
    })