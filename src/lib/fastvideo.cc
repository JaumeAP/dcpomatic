#include "exceptions.h"
#include "fastvideo.h"
#include <fastvideo_decoder_j2k.h>


using boost::shared_ptr;


shared_ptr<OpenJPEGImage>
fastvideo_decompess_j2k (dcp::Data data, int reduce)
{
	fastJ2kImageInfo_t info;
	fastStatus_t r = fastDecoderJ2kPredecode(&info, data.data().get(), data.size());
	if (r != FAST_OK) {
		throw FastvideoError ("J2kPredecode");
	}

	/* Init */

	fastDecoderJ2kStaticParameters_t parameters;
	memset(&parameters, 0, sizeof(fastDecoderJ2kStaticParameters_t));

	parameters.ResolutionLevels = 5 - reduce;
	parameters.verboseLevel = 1;
	parameters.enableROI = 0;

	parameters.maxTileHeight = info.width;
	parameters.maxTileWidth = info.height;

	parameters.windowX0 = 0;
	parameters.windowY0 = 0;
	parameters.windowWidth = info.width;
	parameters.windowHeight = info.height;

	parameters.truncationLength = 0;
	parameters.truncationMode = 0;
	parameters.truncationRate = 0;

	parameters.DecodePasses = 0;
	parameters.imageInfo = &info;
	parameters.maxStreamSize = info.streamSize;

	fastDecoderJ2kHandle_t decoder;
	fastDeviceSurfaceBufferHandle_t buffer;
	r = fastDecoderJ2kCreate(
			&decoder,
			&parameters,
			FAST_RGB8, info.width, info.height,
			1,
			&buffer
			);
	if (r != FAST_OK) {
		throw FastvideoError ("J2kCreate");
	}

	unsigned long long requested_mem_size = 0;

	{
		unsigned long long component_mem_size = 0;
		r = fastDecoderJ2kGetAllocatedGpuMemorySize(decoder, &component_mem_size);
		if (r != FAST_OK) {
			throw FastvideoError ("J2kGetAllocatedGpuMemorySize");
		}
		requested_mem_size += component_mem_size;
	}

	fastExportToHostHandle_t adapter;
	fastSurfaceFormat_t surface_format;
	r = fastExportToHostCreate(&adapter, &surface_format, buffer);
	if (r != FAST_OK) {
		throw FastvideoError ("ExportToHostCreate");
	}

	{
		unsigned int component_mem_size = 0;
		r = fastExportToHostGetAllocatedGpuMemorySize(adapter, &component_mem_size);
		if (r != FAST_OK) {
			throw FastvideoError ("ExportToHostGetAllocatedGpuMemorySize");
		}
		requested_mem_size += component_mem_size;
	}

	const double gigabyte = 1024.0 * 1024.0 * 1024.0;
	printf("\nRequested GPU memory size: %.2lf GB\n", requested_mem_size / gigabyte);

	/* Transform */

	double total_time = 0;
	fastGpuTimerHandle_t device_to_host_timer = 0;
	fastGpuTimerCreate(&device_to_host_timer);

	fastDecoderJ2kReport_t report;

	fastExportParameters_t export_parameters;
	export_parameters.convert = FAST_CONVERT_NONE;

	int aligned_width = info.width * 3;
	aligned_width += 4 - (aligned_width % FAST_ALIGNMENT);
	size_t decoded_size = info.height * aligned_width;
	uint8_t* decoded = 0;
	r = fastMalloc(reinterpret_cast<void**>(&decoded), decoded_size);
	if (r != FAST_OK) {
		throw FastvideoError ("fastMalloc");
	}

	r = fastDecoderJ2kTransform (decoder, decoded, decoded_size, &report);
	if (r != FAST_OK) {
		throw FastvideoError ("J2kTransform");
	}

	total_time += report.elapsedTime;

	fastGpuTimerStart(device_to_host_timer);
	r = fastExportToHostCopy(adapter, decoded, info.width, aligned_width, info.height, &export_parameters);
	if (r != FAST_OK) {
		throw FastvideoError ("ExportToHostCopy");
	}

	float elapsed_time_gpu = 0.;
	fastGpuTimerStop(device_to_host_timer);
	fastGpuTimerGetTime(device_to_host_timer, &elapsed_time_gpu);

	total_time += elapsed_time_gpu / 1000.0;

	if (report.codeblockCount > 0) {
		printf("Input image : (%dx%d pixels; %d %d-bit channel(s))\n", info.width, info.height, report.channels, report.bitsPerChannel);
		printf("Tile count: %d (%dx%d)\n", report.tileCount, report.tilesX, report.tilesY);
		printf("Window mode disabled: decode full image\n");
		printf("Resolution levels: %d\n", report.resolutionLevels);
		printf("Codeblock size: %dx%d\n", report.cbX, report.cbY);

		const double megabyte = 1024.0 * 1024.0;
		printf("%7.2f ms 1) Tier-2 time (%d codeblocks)\n", report.s1_tier2 * 1000.0, report.codeblockCount);
		printf("%7.2f ms 2) CPU->GPU copy time (%.2lf MB, %.2lf MB/s)\n",
			report.s2_copy * 1000.0,
			report.copyToGpu_size / megabyte,
			report.copyToGpu_size == 0 ? 0 : (report.copyToGpu_size / report.s2_copy / megabyte));
		printf("%7.2f ms 3) Tier-1 time\n", report.s3_tier1 * 1000.0);
		printf("%7.2f ms 4) DWT time\n", report.s6_dwt * 1000.0);
		printf("%7.2f ms 5) Postprocessing time (MCT, DC-shift)\n", report.s7_postprocessing * 1000.0);
		printf("              Output size = ");
		if (report.outStreamSize < 1024)
			printf("%lld bytes", report.outStreamSize);
		else
			printf("%.0f KB", report.outStreamSize / 1024.0f);
		printf(" (%.1f:1)\n", report.outStreamSize == 0 ? 0 : (float)report.outStreamSize / report.inStreamSize);
	}

	printf("Total decode time for %d images = %.1f ms; %.1f FPS;\n", 1,
		total_time * 1000.0, 1 / total_time);

	fastGpuTimerDestroy(device_to_host_timer);

	/* Close */

	r = fastDecoderJ2kDestroy(decoder);
	if (r != FAST_OK) {
		throw FastvideoError ("J2kDestroy");
	}
	r = fastExportToHostDestroy(adapter);
	if (r != FAST_OK) {
		throw FastvideoError ("ExportToHostDestroy");
	}

	fastFree(decoded);
}

