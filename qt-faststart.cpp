#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
 
#ifdef __MINGW32__
#define fseeko(x,y,z)  fseeko64(x,y,z)
#define ftello(x)      ftello64(x)
#endif

#define BE_16(x) ((((uint8_t*)(x))[0] << 8) | ((uint8_t*)(x))[1])
#define BE_32(x) ((((uint8_t*)(x))[0] << 24) | \
		(((uint8_t*)(x))[1] << 16) | \
		(((uint8_t*)(x))[2] << 8) | \
		((uint8_t*)(x))[3])
#define BE_64(x) (((uint64_t)(((uint8_t*)(x))[0]) << 56) | \
		((uint64_t)(((uint8_t*)(x))[1]) << 48) | \
		((uint64_t)(((uint8_t*)(x))[2]) << 40) | \
		((uint64_t)(((uint8_t*)(x))[3]) << 32) | \
		((uint64_t)(((uint8_t*)(x))[4]) << 24) | \
		((uint64_t)(((uint8_t*)(x))[5]) << 16) | \
		((uint64_t)(((uint8_t*)(x))[6]) << 8) | \
		((uint64_t)((uint8_t*)(x))[7]))

#define BE_FOURCC( ch0, ch1, ch2, ch3 )             \
	( (uint32_t)(unsigned char)(ch3) |          \
	  ( (uint32_t)(unsigned char)(ch2) << 8 ) |   \
	  ( (uint32_t)(unsigned char)(ch1) << 16 ) |  \
	  ( (uint32_t)(unsigned char)(ch0) << 24 ) )

#define QT_ATOM BE_FOURCC
/* top level atoms */
#define FREE_ATOM QT_ATOM('f', 'r', 'e', 'e')
#define JUNK_ATOM QT_ATOM('j', 'u', 'n', 'k')
#define MDAT_ATOM QT_ATOM('m', 'd', 'a', 't')
#define MOOV_ATOM QT_ATOM('m', 'o', 'o', 'v')
#define PNOT_ATOM QT_ATOM('p', 'n', 'o', 't')
#define SKIP_ATOM QT_ATOM('s', 'k', 'i', 'p')
#define WIDE_ATOM QT_ATOM('w', 'i', 'd', 'e')
#define PICT_ATOM QT_ATOM('P', 'I', 'C', 'T')
#define FTYP_ATOM QT_ATOM('f', 't', 'y', 'p')

#define CMOV_ATOM QT_ATOM('c', 'm', 'o', 'v')
#define STCO_ATOM QT_ATOM('s', 't', 'c', 'o')
#define CO64_ATOM QT_ATOM('c', 'o', '6', '4')

#define ATOM_PREAMBLE_SIZE 8
#define COPY_BUFFER_SIZE 1024


/*
 * qt_faststart 
 * src_path source mp4 path
 * dest_path dest mp4 path
 *  return 0 success -1 failed
*/

int qt_faststart(char * src_path, char * dest_path)
{
	if(!src_path)
	{
		printf("src mp4 path is NULL\n");
		return -1;
	}
	if(!dest_path)
	{
		printf("dest mp4 path is NULL\n");
		return -1;
	}

	FILE *infile;
	FILE *outfile;
	unsigned char atom_bytes[ATOM_PREAMBLE_SIZE];
	uint32_t atom_type = 0;
	uint64_t atom_size = 0;
	uint64_t last_offset;
	unsigned char *moov_atom;
	unsigned char *ftyp_atom = 0;
	uint64_t moov_atom_size;
	uint64_t ftyp_atom_size = 0;
	uint64_t i, j;
	uint32_t offset_count;
	uint64_t current_offset;
	uint64_t start_offset = 0;
	unsigned char copy_buffer[COPY_BUFFER_SIZE];
	int bytes_to_copy;


	infile = fopen(src_path, "rb");
	if (!infile) {
		perror(src_path);
		return -1;
	}

	/* traverse through the atoms in the file to make sure that 'moov' is
	 * at the end */
	while (!feof(infile))
	{
		if (fread(atom_bytes, ATOM_PREAMBLE_SIZE, 1, infile) != 1)
		{
			break;
		}
		atom_size = (uint32_t)BE_32(&atom_bytes[0]);
		atom_type = BE_32(&atom_bytes[4]);

		if ((atom_type != FREE_ATOM) &&
				(atom_type != JUNK_ATOM) &&
				(atom_type != MDAT_ATOM) &&
				(atom_type != MOOV_ATOM) &&
				(atom_type != PNOT_ATOM) &&
				(atom_type != SKIP_ATOM) &&
				(atom_type != WIDE_ATOM) &&
				(atom_type != PICT_ATOM) &&
				(atom_type != FTYP_ATOM)) {

			printf ("encountered non-QT top-level atom (is this a Quicktime file?)\n");
			break;

		}

		/* keep ftyp atom */
		if (atom_type == FTYP_ATOM) {

			ftyp_atom_size = atom_size;
			ftyp_atom = (unsigned char *)malloc(ftyp_atom_size);
			if (!ftyp_atom) {

				printf ("could not allocate 0x%llX byte for ftyp atom\n",
						atom_size);
				fclose(infile);
				return -1;

			}
			//fseeko(infile, -ATOM_PREAMBLE_SIZE, SEEK_CUR);
			fseek(infile, -ATOM_PREAMBLE_SIZE, SEEK_CUR);
			if (fread(ftyp_atom, atom_size, 1, infile) != 1) {

				perror(src_path);
				free(ftyp_atom);
				fclose(infile);
				return -1;

			}
			//start_offset = ftello(infile);
			start_offset = ftell(infile);
			continue;
		}

		/* 64-bit special case */
		if (atom_size == 1) 
		{
			if (fread(atom_bytes, ATOM_PREAMBLE_SIZE, 1, infile) != 1) 
			{
				break;
			}
			atom_size = BE_64(&atom_bytes[0]);
			//fseeko(infile, atom_size - ATOM_PREAMBLE_SIZE * 2, SEEK_CUR);
			fseek(infile, atom_size - ATOM_PREAMBLE_SIZE * 2, SEEK_CUR);
		} else {
			//fseeko(infile, atom_size - ATOM_PREAMBLE_SIZE, SEEK_CUR);
			fseek(infile, atom_size - ATOM_PREAMBLE_SIZE, SEEK_CUR);
		}
	}

	if (atom_type != MOOV_ATOM) {
		printf ("last atom in file was not a moov atom\n");
		//fclose(infile);
		//return -1;
	}

	/* moov atom was, in fact, the last atom in the chunk; load the whole
	 * moov atom */
	int tmp = 0 - int(atom_size);
	//fseeko(infile, tmp, SEEK_END);
	fseek(infile, tmp, SEEK_END);
	//last_offset = ftello(infile);
	last_offset = ftell(infile);
	moov_atom_size = atom_size;
	moov_atom = (unsigned char *)malloc(moov_atom_size);
	if (!moov_atom) {

		printf ("could not allocate 0x%llX byte for moov atom\n",
				atom_size);
		fclose(infile);
		return -1;

	}
	
	if (fread(moov_atom, atom_size, 1, infile) != 1) 
	{
		perror(src_path);
		free(moov_atom);
		fclose(infile);
		return -1;
	}

	/* this utility does not support compressed atoms yet, so disqualify
	 * files with compressed QT atoms */
	if (BE_32(&moov_atom[12]) == CMOV_ATOM) {

		printf ("this utility does not support compressed moov atoms yet\n");
		free(moov_atom);
		fclose(infile);
		return -1;

	}

	/* close; will be re-opened later */
	fclose(infile);

	/* crawl through the moov chunk in search of stco or co64 atoms */
	for (i = 4; i < moov_atom_size - 4; i++) {

		atom_type = BE_32(&moov_atom[i]);
		if (atom_type == STCO_ATOM) {

			printf (" patching stco atom...\n");
			atom_size = BE_32(&moov_atom[i - 4]);
			if (i + atom_size - 4 > moov_atom_size) {

				printf (" bad atom size\n");
				free(moov_atom);
				return -1;

			}
			offset_count = BE_32(&moov_atom[i + 8]);
			for (j = 0; j < offset_count; j++) {

				current_offset = BE_32(&moov_atom[i + 12 + j * 4]);
				current_offset += moov_atom_size;
				moov_atom[i + 12 + j * 4 + 0] = (current_offset >> 24) & 0xFF;
				moov_atom[i + 12 + j * 4 + 1] = (current_offset >> 16) & 0xFF;
				moov_atom[i + 12 + j * 4 + 2] = (current_offset >>  8) & 0xFF;
				moov_atom[i + 12 + j * 4 + 3] = (current_offset >>  0) & 0xFF;

			}
			i += atom_size - 4;

		} else if (atom_type == CO64_ATOM) {

			printf (" patching co64 atom...\n");
			atom_size = BE_32(&moov_atom[i - 4]);
			if (i + atom_size - 4 > moov_atom_size) {

				printf (" bad atom size\n");
				free(moov_atom);
				return -1;

			}
			offset_count = BE_32(&moov_atom[i + 8]);
			for (j = 0; j < offset_count; j++) {

				current_offset = BE_64(&moov_atom[i + 12 + j * 8]);
				current_offset += moov_atom_size;
				moov_atom[i + 12 + j * 8 + 0] = (current_offset >> 56) & 0xFF;
				moov_atom[i + 12 + j * 8 + 1] = (current_offset >> 48) & 0xFF;
				moov_atom[i + 12 + j * 8 + 2] = (current_offset >> 40) & 0xFF;
				moov_atom[i + 12 + j * 8 + 3] = (current_offset >> 32) & 0xFF;
				moov_atom[i + 12 + j * 8 + 4] = (current_offset >> 24) & 0xFF;
				moov_atom[i + 12 + j * 8 + 5] = (current_offset >> 16) & 0xFF;
				moov_atom[i + 12 + j * 8 + 6] = (current_offset >>  8) & 0xFF;
				moov_atom[i + 12 + j * 8 + 7] = (current_offset >>  0) & 0xFF;

			}
			i += atom_size - 4;

		}

	}

	/* re-open the input file and open the output file */
	infile = fopen(src_path, "rb");
	if (!infile) {
		perror(src_path);
		free(moov_atom);
		return -1;

	}

	if (start_offset > 0) {
		/* seek after ftyp atom */
		//fseeko(infile, start_offset, SEEK_SET);
		fseek(infile, start_offset, SEEK_SET);
		last_offset -= start_offset;

	}

	outfile = fopen(dest_path, "wb");
	if (!outfile) 
	{
		perror(dest_path);
		fclose(outfile);
		free(moov_atom);
		return -1;
	}

	/* dump the same ftyp atom */
	if (ftyp_atom_size > 0) {

		printf (" writing ftyp atom... [%d] \n", ftyp_atom_size);
		if (fwrite(ftyp_atom, ftyp_atom_size, 1, outfile) != 1) {
			perror(dest_path);
			goto error_out;

		}

	}

	/* dump the new moov atom */
	printf(" writing moov atom... [%d] \n", moov_atom_size);
	if (fwrite(moov_atom, moov_atom_size, 1, outfile) != 1) {

		perror(dest_path);
		goto error_out;

	}

	/* copy the remainder of the infile, from offset 0 -> last_offset - 1 */
	printf (" copying rest of file...\n");
	int rest_count = 0;
	while (last_offset) {

		if (last_offset > COPY_BUFFER_SIZE)
			bytes_to_copy = COPY_BUFFER_SIZE;
		else
			bytes_to_copy = last_offset;
			
		if (fread(copy_buffer, bytes_to_copy, 1, infile) != 1) {

			perror(src_path);
			goto error_out;

		}
		
		rest_count += bytes_to_copy;
		if (fwrite(copy_buffer, bytes_to_copy, 1, outfile) != 1) {

			perror(dest_path);
			goto error_out;

		}

		last_offset -= bytes_to_copy;
	}
	
	printf (" rest : %d \n", rest_count);

	fclose(infile);
	fclose(outfile);
	free(moov_atom);
	if (ftyp_atom_size > 0)
		free(ftyp_atom);

	return 0;

error_out:
	fclose(infile);
	fclose(outfile);
	free(moov_atom);
	if (ftyp_atom_size > 0)
		free(ftyp_atom);
	return -1;
}

int parseMp4Info(char *filename)
{
	FILE *infile;
	unsigned char atom_bytes[ATOM_PREAMBLE_SIZE];
	uint32_t atom_type = 0;
	uint64_t atom_size = 0;
	uint64_t last_offset;
	unsigned char *moov_atom;
	unsigned char *ftyp_atom = 0;
	uint64_t moov_atom_size;
	uint64_t ftyp_atom_size = 0;
	uint64_t i, j;
	uint32_t offset_count;
	uint64_t current_offset;
	uint64_t start_offset = 0;
	
	int ftypPos = 0, ftypSize = 0;
	int moovPos = 0, moovSize = 0;

	infile = fopen(filename, "rb");
	if (!infile) {
		perror(filename);
		return -1;
	}

	/* traverse through the atoms in the file to make sure that 'moov' is
	 * at the end */
	while (!feof(infile))
	{
		if (fread(atom_bytes, ATOM_PREAMBLE_SIZE, 1, infile) != 1)
		{
			break;
		}
		atom_size = (uint32_t)BE_32(&atom_bytes[0]);
		atom_type = BE_32(&atom_bytes[4]);

		if ((atom_type != FREE_ATOM) &&
				(atom_type != JUNK_ATOM) &&
				(atom_type != MDAT_ATOM) &&
				(atom_type != MOOV_ATOM) &&
				(atom_type != PNOT_ATOM) &&
				(atom_type != SKIP_ATOM) &&
				(atom_type != WIDE_ATOM) &&
				(atom_type != PICT_ATOM) &&
				(atom_type != FTYP_ATOM)) {

			printf ("encountered non-QT top-level atom (is this a Quicktime file?)\n");
			break;

		}

		/* keep ftyp atom */
		if (atom_type == FTYP_ATOM) {

			ftyp_atom_size = atom_size;
			ftyp_atom = (unsigned char *)malloc(ftyp_atom_size);
			if (!ftyp_atom) {

				printf ("could not allocate 0x%llX byte for ftyp atom\n",
						atom_size);
				fclose(infile);
				return -1;

			}
			//fseeko(infile, -ATOM_PREAMBLE_SIZE, SEEK_CUR);
			fseek(infile, -ATOM_PREAMBLE_SIZE, SEEK_CUR);
			ftypPos = ftell(infile);
			ftypSize = atom_size;
			if (fread(ftyp_atom, atom_size, 1, infile) != 1) {

				perror(filename);
				free(ftyp_atom);
				fclose(infile);
				return -1;

			}
			//start_offset = ftello(infile);
			start_offset = ftell(infile);
			continue;
		}

		/* 64-bit special case */
		if (atom_size == 1) 
		{
			if (fread(atom_bytes, ATOM_PREAMBLE_SIZE, 1, infile) != 1) 
			{
				break;
			}
			atom_size = BE_64(&atom_bytes[0]);
			//fseeko(infile, atom_size - ATOM_PREAMBLE_SIZE * 2, SEEK_CUR);
			fseek(infile, atom_size - ATOM_PREAMBLE_SIZE * 2, SEEK_CUR);
		} else {
			//fseeko(infile, atom_size - ATOM_PREAMBLE_SIZE, SEEK_CUR);
			fseek(infile, atom_size - ATOM_PREAMBLE_SIZE, SEEK_CUR);
		}
	}

	if (atom_type != MOOV_ATOM) {
		printf ("last atom in file was not a moov atom\n");
		//fclose(infile);
		//return -1;
	}

	/* moov atom was, in fact, the last atom in the chunk; load the whole
	 * moov atom */
	int tmp = 0 - int(atom_size);
	//fseeko(infile, tmp, SEEK_END);
	fseek(infile, tmp, SEEK_END);
	//last_offset = ftello(infile);
	last_offset = ftell(infile);
	moov_atom_size = atom_size;
	moov_atom = (unsigned char *)malloc(moov_atom_size);
	if (!moov_atom) {

		printf ("could not allocate 0x%llX byte for moov atom\n",
				atom_size);
		fclose(infile);
		return -1;

	}
	
	moovPos = ftell(infile);
	moovSize = atom_size;
	if (fread(moov_atom, atom_size, 1, infile) != 1) 
	{
		perror(filename);
		free(moov_atom);
		fclose(infile);
		return -1;
	}
	
	free(moov_atom);
	if (ftyp_atom_size > 0)
		free(ftyp_atom);
	fclose(infile);
	
	printf("\n 文件头信息如下 : \n");
	printf("\tftyp pos : %d, ftyp size : %d\n", ftypPos, ftypSize);
	printf("\tmoov pos : %d, moov size : %d\n\n", moovPos, moovSize);
	
	return 0;
}

int main(int argc, char *argv[])
{
	if (argc != 3 && argc!=2) {
		printf ("Usage: mp4-info <infile.mp4> [outfile.mp4]\n");
		return 0;
	}

	int ret = 0;

	if (argc==2)
		ret = parseMp4Info(argv[1]);
	else
		ret = qt_faststart(argv[1], argv[2]);

	printf("ret = %d\n", ret);


	return 0;
}
