

const img = document.getElementsByTagName('img')[0];
//img.onload = () => {
    img.src = '/video?' + new Date().getTime();
//};

// const ws = new WebSocket('/ws');
// ws.binaryType = 'arraybuffer';

// const canvas = document.getElementsByTagName('canvas')[0];
// const context = canvas.getContext('2d');
// let latest = performance.now();

// ws.onmessage = (ev) => {
    
//     const blob = new Blob([event.data], { type: 'image/jpeg' });
//     const img = new Image();

//     img.onload = function() {
//         canvas.width = img.width;
//         canvas.height = img.height;
//         context.drawImage(img, 0, 0);
//     };

//     img.src = URL.createObjectURL(blob);
   
//     const now = performance.now();
//     console.log(`Take: ${now - latest}`);
//     latest = performance.now();
// }

// ws.onopen = () => {
//     console.log('WebSocket connected');
// };

// ws.onclose = () => {
//     console.log('WebSocket closed');
// };

// window.wsSend = (data) => {
//     ws.send(data);
// }