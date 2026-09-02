// SPDX-License-Identifier: GPL-2.0-or-later
#include "rend/neural/neural_stage.h"

#include <iostream>
#include <string>

int main(int argc, char **argv)
{
	using namespace flycast::rend::neural;
	if (argc == 2 && std::string(argv[1]) == "--version")
	{
		std::cout << "neuraltest phase-0\n";
		return 0;
	}
	std::cout << "neuraltest: render determinism scaling depth motion neural compare capture\n";
	return 0;
}
