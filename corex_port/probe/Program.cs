using System;
using System.IO;
using ILGPU;
using ILGPU.Runtime;
using ILGPU.Runtime.Cuda;
using ILGPU.Backends;
using ILGPU.Backends.PTX;

class Probe
{
    static void Kernel(Index1D i, ArrayView<int> data)
    {
        data[i] = i + 1;
    }

    static int Main()
    {
        Console.WriteLine("=== ILGPU CUDA probe on CoreX ivcore11 ===");
        System.Diagnostics.Trace.Listeners.Add(
            new System.Diagnostics.TextWriterTraceListener(Console.Out));
        using var context = Context.Create(b => b.Cuda());
        var dev = context.GetCudaDevice(0);
        Console.WriteLine($"Device: {dev.Name} arch={dev.Architecture} isa={dev.InstructionSet} warp={dev.WarpSize}");

        CudaAccelerator accel = null;
        try
        {
            accel = context.CreateCudaAccelerator(0);
            Console.WriteLine($"Accelerator created: {accel.Name}");
        }
        catch (Exception ex)
        {
            Console.WriteLine("STEP=CreateAccelerator FAILED: " + ex.GetType().FullName + ": " + ex.Message);
            Console.WriteLine(ex.StackTrace);
            return 4;
        }

        // Step: compile kernel to PTX and dump it
        try
        {
            var entry = typeof(Probe).GetMethod(nameof(Kernel),
                System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Static);
            var backend = accel.Backend;
            Console.WriteLine("Backend type: " + backend.GetType().FullName);
        }
        catch (Exception ex)
        {
            Console.WriteLine("STEP=BackendInfo note: " + ex.Message);
        }

        // Compile the kernel to PTX and dump it, so we can inspect the exact
        // NVIDIA-style PTX text ILGPU hands to cuModuleLoadData on CoreX.
        try
        {
            var method = typeof(Probe).GetMethod(nameof(Kernel),
                System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Static);
            var backend = (ILGPU.Backends.PTX.PTXBackend)accel.Backend;
            var entryPoint = ILGPU.Backends.EntryPoints.EntryPointDescription
                .FromImplicitlyGroupedKernel(method);
            var compiled = (ILGPU.Backends.PTX.PTXCompiledKernel)backend.Compile(
                entryPoint, default);
            var ptxPath = "/home/repos/ILGPU/corex_port/build/ilgpu_generated.ptx";
            File.WriteAllText(ptxPath, compiled.PTXAssembly);
            Console.WriteLine("PTX dumped to " + ptxPath +
                " (" + compiled.PTXAssembly.Length + " bytes)");
            var head = compiled.PTXAssembly;
            var firstLines = head.Split('\n');
            for (int i = 0; i < Math.Min(8, firstLines.Length); i++)
                Console.WriteLine("  PTX> " + firstLines[i].TrimEnd());
        }
        catch (Exception ex)
        {
            Console.WriteLine("STEP=DumpPTX note: " + ex.GetType().Name + ": " + ex.Message);
        }

        Action<Index1D, ArrayView<int>> launcher = null;
        try
        {
            var kernel = accel.LoadAutoGroupedStreamKernel<Index1D, ArrayView<int>>(Kernel);
            launcher = kernel;
            Console.WriteLine("STEP=LoadKernel OK (PTX compiled + module loaded)");
        }
        catch (Exception ex)
        {
            Console.WriteLine("STEP=LoadKernel FAILED: " + ex.GetType().FullName + ": " + ex.Message);
            Console.WriteLine(ex.StackTrace);
            Environment.Exit(5);
        }

        try
        {
            const int N = 64;
            using var buf = accel.Allocate1D<int>(N);
            launcher((int)N, buf.View);
            accel.Synchronize();
            var host = buf.GetAsArray1D();
            bool ok = true;
            for (int i = 0; i < N; i++)
                if (host[i] != i + 1) { ok = false; Console.WriteLine($"  mismatch at {i}: got {host[i]} want {i+1}"); break; }
            Console.WriteLine(ok ? $"STEP=Launch KERNEL RESULT OK (first={host[0]}, last={host[N-1]})" : "STEP=Launch KERNEL RESULT WRONG");
            Environment.Exit(ok ? 0 : 3);
        }
        catch (Exception ex)
        {
            Console.WriteLine("STEP=Launch FAILED: " + ex.GetType().FullName + ": " + ex.Message);
            Console.WriteLine(ex.StackTrace);
            Environment.Exit(6);
        }
        return 0;
    }
}
