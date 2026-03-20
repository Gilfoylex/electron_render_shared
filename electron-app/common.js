window.initWebGpu = async () => {
  // Init WebGPU
  const canvas = document.createElement('canvas');
  canvas.width = 128;
  canvas.height = 128;
  canvas.style.width = '128px';
  canvas.style.height = '128px';
  canvas.style.position = 'absolute';
  canvas.style.top = '16px';
  canvas.style.left = '16px';

  document.body.appendChild(canvas);
  const context = canvas.getContext('webgpu');

  // Configure WebGPU context
  const adapter = await navigator.gpu.requestAdapter();
  const device = await adapter.requestDevice();
  const format = navigator.gpu.getPreferredCanvasFormat();
  context.configure({ device, format });

  window.renderFrame = async (frame) => {
    try {
      // Create external texture
      const externalTexture = device.importExternalTexture({ source: frame });

      // Create bind group layout, correctly specifying the external texture type
      const bindGroupLayout = device.createBindGroupLayout({
        entries: [
          {
            binding: 0,
            visibility: window.GPUShaderStage.FRAGMENT,
            externalTexture: {}
          },
          {
            binding: 1,
            visibility: window.GPUShaderStage.FRAGMENT,
            sampler: {}
          }
        ]
      });

      // Create pipeline layout
      const pipelineLayout = device.createPipelineLayout({
        bindGroupLayouts: [bindGroupLayout]
      });

      // Create render pipeline
      const pipeline = device.createRenderPipeline({
        layout: pipelineLayout,
        vertex: {
          module: device.createShaderModule({
            code: `
                @vertex
                fn main(@builtin(vertex_index) VertexIndex : u32) -> @builtin(position) vec4<f32> {
                var pos = array<vec2<f32>, 6>(
                    vec2<f32>(-1.0, -1.0),
                    vec2<f32>(1.0, -1.0),
                    vec2<f32>(-1.0, 1.0),
                    vec2<f32>(-1.0, 1.0),
                    vec2<f32>(1.0, -1.0),
                    vec2<f32>(1.0, 1.0)
                );
                return vec4<f32>(pos[VertexIndex], 0.0, 1.0);
                }
            `
          }),
          entryPoint: 'main'
        },
        fragment: {
          module: device.createShaderModule({
            code: `
                @group(0) @binding(0) var extTex: texture_external;
                @group(0) @binding(1) var mySampler: sampler;

                @fragment
                fn main(@builtin(position) fragCoord: vec4<f32>) -> @location(0) vec4<f32> {
                let texCoord = fragCoord.xy / vec2<f32>(${canvas.width}.0, ${canvas.height}.0);
                return textureSampleBaseClampToEdge(extTex, mySampler, texCoord);
                }
            `
          }),
          entryPoint: 'main',
          targets: [{ format }]
        },
        primitive: { topology: 'triangle-list' }
      });

      // Create bind group
      const bindGroup = device.createBindGroup({
        layout: bindGroupLayout,
        entries: [
          {
            binding: 0,
            resource: externalTexture
          },
          {
            binding: 1,
            resource: device.createSampler()
          }
        ]
      });

      // Create command encoder and render pass
      const commandEncoder = device.createCommandEncoder();
      const textureView = context.getCurrentTexture().createView();
      const renderPass = commandEncoder.beginRenderPass({
        colorAttachments: [
          {
            view: textureView,
            clearValue: { r: 0.0, g: 0.0, b: 0.0, a: 1.0 },
            loadOp: 'clear',
            storeOp: 'store'
          }
        ]
      });

      // Set pipeline and bind group
      renderPass.setPipeline(pipeline);
      renderPass.setBindGroup(0, bindGroup);
      renderPass.draw(6); // Draw a rectangle composed of two triangles
      renderPass.end();

      // Submit commands
      device.queue.submit([commandEncoder.finish()]);
    } catch (error) {
      console.error('Rendering error:', error);
    }
  };
};