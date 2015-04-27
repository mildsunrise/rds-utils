#include <stdio.h>

#include "EncoderRDS.h"

int main(int argc, char **argv) {
  RDS_Utils::EncoderRDS encoder;
  if (encoder.readXML(argv[1])) return 1;
  encoder.encode();
  for (int i = 0; i < encoder.nbuffers; i++) {
    fwrite(encoder.buffer[i], sizeof(char), 104, stdout);
  }
  return 0;
}
