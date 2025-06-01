/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/


#include <dcp/openjpeg_image.h>
#include <dcp/j2k_transcode.h>
#include <dcp/raw_convert.h>
#include <dcp/types.h>
#include <fmt/format.h>
#include <boost/algorithm/string.hpp>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>


using std::string;
using std::vector;


auto constexpr CLIENT_TO_GROK_MESSAGE = "Global\\client_to_grok_message";
auto constexpr GROK_TO_CLIENT_MESSAGE = "Global\\grok_to_client_message";
auto constexpr GROK_UNCOMPRESSED_BUFFER = "Global\\grok_uncompressed_buf";
auto constexpr GROK_COMPRESSED_BUFFER = "Global\\grok_compressed_buf";
auto constexpr CLIENT_SENT = "Global\\client_sent";
auto constexpr CLIENT_RECEIVE_READY = "Global\\client_receive_ready";
auto constexpr GROK_SENT = "Global\\grok_sent";
auto constexpr GROK_RECEIVE_READY = "Global\\grok_receive_ready";
auto constexpr GROK_BATCH_COMPRESS_INIT = "GRK_MSGR_BATCH_COMPRESS_INIT";
auto constexpr GROK_PROCESSED_COMPRESSED = "GRK_MSGR_BATCH_PROCESSSED_COMPRESSED";
auto constexpr GROK_PROCESSED_UNCOMPRESSED = "GRK_MSGR_BATCH_PROCESSED_UNCOMPRESSED";
auto constexpr GROK_SUBMIT_COMPRESSED = "GRK_MSGR_BATCH_SUBMIT_COMPRESSED";
auto constexpr GROK_SUBMIT_UNCOMPRESSED = "GRK_MSGR_BATCH_SUBMIT_UNCOMPRESSED";
auto constexpr GROK_SHUTDOWN = "GRK_MSGR_BATCH_SHUTDOWN";
auto constexpr MESSAGE_BUFFER_LEN = 256;


int client_to_grok_message_fd;
char* client_to_grok_message_buffer;
int grok_to_client_message_fd;
char* grok_to_client_message_buffer;
int grok_uncompressed_fd;
char* grok_uncompressed_buffer;
int grok_compressed_fd;
char* grok_compressed_buffer;
sem_t* client_receive_ready;
sem_t* grok_sent;


static void init_shm(char const* name, int length, int* fd, char** buffer)
{
	*fd = shm_open(name, O_CREAT | O_RDWR, 0666);
	if (*fd < 0) {
		std::cerr << "shm_open " << name << " failed: " << strerror(errno) << "\n";
		exit(EXIT_FAILURE);
	}

	if (ftruncate(*fd, length) < 0) {
		std::cerr << "ftruncate failed: " << strerror(errno) << "\n";
		exit(EXIT_FAILURE);
	}

	*buffer = static_cast<char*>(mmap(0, length, PROT_WRITE, MAP_SHARED, *fd, 0));
	if (*buffer == reinterpret_cast<void*>(-1)) {
		std::cerr << "mmap failed: " << strerror(errno) << "\n";
		close(*fd);
		shm_unlink(name);
		exit(EXIT_FAILURE);
	}
}


static void send(char const* message)
{
	if (sem_wait(client_receive_ready)) {
		std::cout << "sem_wait failed\n";
		exit(EXIT_FAILURE);
	}

	strcpy(grok_to_client_message_buffer, message);
	if (sem_post(grok_sent)) {
		std::cout << "sem_post failed\n";
		exit(EXIT_FAILURE);
	}
}


int main(int argc, char* argv[])
{
	std::cout << "Mock Grok starting.\n";
	if (argc < 3) {
		std::cerr << "Missing parameter.\n";
		exit(EXIT_FAILURE);
	}

	vector<string> parameters;
	boost::algorithm::split(parameters, argv[2], boost::is_any_of(","));
	std::cout << "Parameters: " << argv[2] << "\n";

	string licence;
	if (argc >= 12) {
		licence = argv[12];
	}

	std::cout << "Licence: " << licence << "\n";

	if (parameters.size() != 6) {
		std::cerr << "Unexpected parameter count.\n";
		exit(EXIT_FAILURE);
	}

	int const width = dcp::raw_convert<int>(parameters[1]);
	int const stride = dcp::raw_convert<int>(parameters[2]);
	int const height = dcp::raw_convert<int>(parameters[3]);
	int const samples_per_pixel = dcp::raw_convert<int>(parameters[4]);
	int const depth = dcp::raw_convert<int>(parameters[5]);

	int const compressed_frame_size = 10 * 1024 * 1024;
	int const num_frames = 16;

	init_shm(CLIENT_TO_GROK_MESSAGE, MESSAGE_BUFFER_LEN, &client_to_grok_message_fd, &client_to_grok_message_buffer);
	init_shm(GROK_TO_CLIENT_MESSAGE, MESSAGE_BUFFER_LEN, &grok_to_client_message_fd, &grok_to_client_message_buffer);

	init_shm(
		GROK_UNCOMPRESSED_BUFFER,
		width * height * samples_per_pixel * num_frames * 2,
		&grok_uncompressed_fd,
		&grok_uncompressed_buffer
	);

	init_shm(GROK_COMPRESSED_BUFFER, compressed_frame_size * num_frames, &grok_compressed_fd, &grok_compressed_buffer);

	auto dummy = std::make_shared<dcp::OpenJPEGImage>(dcp::Size{width, height});
	for (int i = 0; i < (width * height); ++i) {
		dummy->data(0)[i] = rand() % 4096;
		dummy->data(1)[i] = rand() % 4096;
		dummy->data(2)[i] = rand() % 4096;
	}
	auto j2k = dcp::compress_j2k(dummy, 50000000, 24, false, false);
	if (j2k.size() > compressed_frame_size) {
		std::cerr << "Dummy frame is too big.\n";
		exit(EXIT_FAILURE);
	}
	for (auto i = 0; i < num_frames; ++i) {
		memcpy(grok_compressed_buffer + i * compressed_frame_size, j2k.data(), j2k.size());
	}

	auto client_sent = sem_open(CLIENT_SENT, O_CREAT, 0666, 0);
	if (client_sent == SEM_FAILED) {
		std::cerr << "sem_open failed for " << CLIENT_SENT << "\n";
		exit(EXIT_FAILURE);
	}

	client_receive_ready = sem_open(CLIENT_RECEIVE_READY, O_CREAT, 0666, 1);
	if (client_receive_ready == SEM_FAILED) {
		std::cerr << "sem_open failed for " << CLIENT_SENT << "\n";
		exit(EXIT_FAILURE);
	}

	grok_sent = sem_open(GROK_SENT, O_CREAT, 0666, 0);
	if (grok_sent == SEM_FAILED) {
		std::cerr << "sem_open failed for " << GROK_SENT << "\n";
		exit(EXIT_FAILURE);
	}

	auto grok_receive_ready = sem_open(GROK_RECEIVE_READY, O_CREAT, 0666, 1);
	if (grok_receive_ready == SEM_FAILED) {
		std::cerr << "sem_open failed for " << GROK_RECEIVE_READY << "\n";
		exit(EXIT_FAILURE);
	}

	std::cout << "Mock Grok ready.\n";

	auto init = fmt::format(
		"{},{},{},{},{},{},{},{}",
		GROK_BATCH_COMPRESS_INIT,
		width,
		stride,
		height,
		samples_per_pixel,
		depth,
		compressed_frame_size,
		num_frames
	);
	send(init.c_str());

	int frames = 0;
	while (true) {
		if (sem_wait(client_sent)) {
			std::cout << "sem_wait failed\n";
			exit(EXIT_FAILURE);
		}

		vector<string> parameters;
		boost::algorithm::split(parameters, client_to_grok_message_buffer, boost::is_any_of(","));

		if (parameters[0] == GROK_SUBMIT_UNCOMPRESSED) {
			if (parameters.size() != 3) {
				std::cerr << "Unexpected parameter count in " << client_to_grok_message_buffer << "\n";
				exit(EXIT_FAILURE);
			}
			++frames;

			int const dcp_frame = dcp::raw_convert<int>(parameters[1]);
			int const frame_id = dcp::raw_convert<int>(parameters[2]);
			send(fmt::format("{},{}", GROK_PROCESSED_UNCOMPRESSED, frame_id).c_str());
			if (licence == "random_drop" && (rand() % 16 != 0)) {
				send(fmt::format("{},{},{},{}", GROK_SUBMIT_COMPRESSED, dcp_frame, frame_id, j2k.size()).c_str());
			}
		} else if (parameters[0] == GROK_SHUTDOWN) {
			break;
		}

		if (sem_post(grok_receive_ready)) {
			std::cout << "sem_post failed\n";
			exit(EXIT_FAILURE);
		}
	}

	sem_close(client_sent);
	sem_close(client_receive_ready);
	sem_close(grok_sent);
	sem_close(grok_receive_ready);

	sem_unlink(CLIENT_SENT);
	sem_unlink(CLIENT_RECEIVE_READY);
	sem_unlink(GROK_SENT);
	sem_unlink(GROK_RECEIVE_READY);

	return 0;
}

