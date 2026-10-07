/**
 * Autonomous WebGPU Pipeline Launcher for kernel: vec_add
 */
export async function launch_vec_add(device, workgroups_x, workgroups_y, ...buffers) {
    const shaderModule = device.createShaderModule({ code: WGSL_SHADER_CODE });
    const bindGroupLayoutEntries = buffers.map((_, i) => ({
        binding: i,
        visibility: GPUShaderStage.COMPUTE,
        buffer: { type: 'storage' }
    }));
    const bindGroupLayout = device.createBindGroupLayout({ entries: bindGroupLayoutEntries });
    const pipelineLayout = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });
    const computePipeline = device.createComputePipeline({
        layout: pipelineLayout,
        compute: { module: shaderModule, entryPoint: 'main' }
    });
    const bindGroup = device.createBindGroup({
        layout: bindGroupLayout,
        entries: buffers.map((b, i) => ({ binding: i, resource: { buffer: b } }))
    });
    const commandEncoder = device.createCommandEncoder();
    const passEncoder = commandEncoder.beginComputePass();
    passEncoder.setPipeline(computePipeline);
    passEncoder.setBindGroup(0, bindGroup);
    passEncoder.dispatchWorkgroups(workgroups_x, workgroups_y || 1, 1);
    passEncoder.end();
    device.queue.submit([commandEncoder.finish()]);
}
