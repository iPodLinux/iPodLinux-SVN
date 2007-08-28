/* Copyright (c) 2005 Joshua Oreman.
 * You may do absolutely anything you want with this code.
 *
 * Compile with
 *  cc -o iplsign iplsign.c -lcrypto
 * Put the private key in file "iplsign.key"
 * Sign things with
 *  ./iplsign "modname-Module Display Name-Author"
 * and put the output in the Module file.
 */

#include <openssl/rsa.h>
#include <openssl/engine.h>
#include <openssl/objects.h>

RSA *pubkey, *privkey;

void usage (int e) 
{
	fprintf (stderr, "Usage: iplkey sign <text>\n"
                     "       iplkey verify <text> <key>\n");
	exit(e);
}

int hex2nyb (char c) 
{
	c = toupper (c);
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return 0;
}

int main (int argc, char **argv)
{
	char md5[16];
	FILE *pf = fopen ("iplsign.key", "r");
	char buf[80];
	int pa = 0;
	if (pf) {
		fgets (buf, 80, pf);
		fclose (pf);
		pa = 1;
	}
	pubkey = RSA_new();
	privkey = RSA_new();
	memset (pubkey, 8 * sizeof(BIGNUM*), 0);
	memset (privkey, 8 * sizeof(BIGNUM*), 0);
	BN_hex2bn (&pubkey->n, "00db4c8c9eaf7904df800bb62eeb4a77f1");
	BN_hex2bn (&pubkey->e, "11");
	BN_hex2bn (&privkey->n, "00db4c8c9eaf7904df800bb62eeb4a77f1");
	BN_hex2bn (&privkey->e, "11");
	BN_hex2bn (&privkey->d, buf);

	if (argc < 3)
		usage(1);

	MD5 (argv[2], strlen (argv[2]), md5);

	// Oddity here: The data must be the same length as the modulus,
	// but can't be greater than it (numerically). Our modulus' top
	// nibble is D = 1101; thus, ANDing the top nibble with ~2 = 0xD will
	// produce good results

	md5[0] &= 0xD;

	if (!strcmp (argv[1], "sign")) {
		if (!pa) {
			fprintf (stderr, "No private key available.\n");
			return 1;
		}
		unsigned char *sig = malloc (RSA_size (privkey));
		int siglen, i;
		siglen = RSA_private_encrypt (16, md5, sig, privkey, RSA_NO_PADDING);
		for (i = 0; i < siglen; i++) {
			printf ("%02x", sig[i]);
		}
		printf ("\n");
	} else if (!strcmp (argv[1], "verify") && (argc >= 4)) {
		unsigned char *sig = malloc (strlen (argv[3]));
		unsigned char *decrypted = malloc (4096);
		int siglen = 0, i;
		for (i = 0; i < strlen (argv[3]); ) {
			int byte = 0;
			byte <<= 4; byte |= hex2nyb (argv[3][i++]);
			byte <<= 4; byte |= hex2nyb (argv[3][i++]);
			sig[siglen++] = byte;
		}
		siglen = RSA_public_decrypt (siglen, sig, decrypted, pubkey, RSA_NO_PADDING);
		if (siglen != 16) {
			printf ("odd siglen ret'd: %d\n", siglen);
			return 2;
		}
		if (memcmp (md5, decrypted, 16) != 0) {
			printf ("NOT OK\n");
			return 1;
		}
		printf ("ok\n");
	}

	return 0;
}
