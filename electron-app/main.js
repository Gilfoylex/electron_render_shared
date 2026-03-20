const { app, BrowserWindow, sharedTexture, ipcMain } = require("electron");
const path = require('node:path');
const crypto = require('node:crypto')

const capturedTextures = new Map();

const createWindow = () => {
  const win = new BrowserWindow({
    width: 800,
    height: 800,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js')
    }
  })

  const osr = new BrowserWindow({
    width: 128,
    height: 128,
    show: false,
    webPreferences: {
      offscreen: {
        useSharedTexture: true
      }
    }
  })
  osr.webContents.setFrameRate(1);
  osr.webContents.on('paint', (event) => {
    // Step 1: Input source of shared texture handle.
    //const texture = event.texture;
    //console.log(JSON.stringify(texture.textureInfo, null, 2));

    const handleValue = 0x0000000040004642n; // 必须用 BigInt

    const ntHandle = Buffer.alloc(8);
    ntHandle.writeBigUInt64LE(handleValue);

    const mytextureinfo = {
      pixelFormat: 'bgra',
      codedSize: {
        width: 192,
        height: 192
      },
      visibleRect: {
        x: 0,
        y: 0,
        width: 192,
        height: 192
      },
      contentRect: {
        x: 0,
        y: 0,
        width: 192,
        height: 192
      },
      timestamp: 0,
      colorSpace: {
        primaries: 'bt709',
        transfer: 'srgb',
        matrix: 'rgb',
        range: 'full'
      },
      widgetType: 'frame',
      metadata: {
        captureUpdateRect: {
          x: 0,
          y: 0,
          width: 192,
          height: 192
        },
        regionCaptureRect: null,
        sourceSize: {
          width: 192,
          height: 192
        },
        frameCount: 0
      },
      handle: {
        ntHandle: ntHandle
      }
    };

    const texture = {
      textureInfo: mytextureinfo
    };
    if (!texture) {
      console.error('No texture, GPU may be unavailable, skipping.')
    }

    // Step 2: Import as SharedTextureImported
    const importedSubtle = sharedTexture.subtle.importSharedTexture(texture.textureInfo);

    // Step 3: Prepare for transfer to another process (win's renderer)
    const transfer = importedSubtle.startTransferSharedTexture();

    const id = crypto.randomUUID();
    capturedTextures.set(id, { importedSubtle, texture });

    // Step 4: Send the shared texture to the renderer process (goto preload.js)
    win.webContents.send('shared-texture', id, transfer);
  })


  //win.loadFile('index.html')
  win.loadFile('index_shared_texture.html')
  win.webContents.openDevTools()
  osr.loadFile(path.join(__dirname, 'osr.html'))
}

app.whenReady().then(() => {
  createWindow()
}).catch((err) => {
  console.error('Application failed to start:', err);
});