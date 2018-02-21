#!/usr/bin/env python

from gnuradio import gr
from gnuradio import blocks
from gnuradio.eng_option import eng_option
from optparse import OptionParser
import LTE_fdd_dl_fg
import sys

class LTE_fdd_dl_file_gen(gr.top_block):

    def __init__(self):
        gr.top_block.__init__(self)

        usage = "usage: %prog [options] file"
        parser=OptionParser(option_class=eng_option, usage=usage)
        # Add options here
        parser.add_option("-d", "--data-type", type="string", default="int8",
                          help="Output file data type, default=%default, options=[int8, gr_complex]")
        (options, args) = parser.parse_args()
        if len(args) != 1:
            parser.print_help()
            sys.exit(1)

        if options.data_type == "gr_complex":
            size = gr.sizeof_gr_complex
        elif options.data_type == "int8":
            size = gr.sizeof_char
        else:
            print("Invalid data type using int8")
            size = gr.sizeof_char

        output_filename = args[0]

        # Build flow graph
        self.samp_buf = LTE_fdd_dl_fg.samp_buf(size)
        self.fsink = blocks.file_sink(size, output_filename)
        self.connect(self.samp_buf, self.fsink)

if __name__ == '__main__':
    tb = LTE_fdd_dl_file_gen()
    try:
        tb.run()
    except KeyboardInterrupt:
        pass
