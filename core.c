
#include "picocoin-config.h"

#include "picocoin.h"
#include "core.h"
#include "coredefs.h"
#include "serialize.h"

bool deser_bp_addr(unsigned int protover,
		struct bp_address *addr, struct buffer *buf)
{
	if (protover >= CADDR_TIME_VERSION)
		if (!deser_u32(&addr->nTime, buf)) return false;
	if (!deser_u64(&addr->nServices, buf)) return false;
	if (!deser_bytes(&addr->ip, buf, 16)) return false;
	if (!deser_u16(&addr->port, buf)) return false;
	return true;
}

void ser_bp_addr(GString *s, unsigned int protover, const struct bp_address *addr)
{
	if (protover >= CADDR_TIME_VERSION) 
		ser_u32(s, addr->nTime);
	ser_u64(s, addr->nServices);
	ser_bytes(s, addr->ip, 16);
	ser_u16(s, addr->port);
}

void bp_outpt_init(struct bp_outpt *outpt)
{
	memset(outpt, 0, sizeof(*outpt));
	BN_init(&outpt->hash);
}

bool deser_bp_outpt(struct bp_outpt *outpt, struct buffer *buf)
{
	if (!deser_u256(&outpt->hash, buf)) return false;
	if (!deser_u32(&outpt->n, buf)) return false;
	return true;
}

void ser_bp_outpt(GString *s, const struct bp_outpt *outpt)
{
	ser_u256(s, &outpt->hash);
	ser_u32(s, outpt->n);
}

void bp_outpt_free(struct bp_outpt *outpt)
{
	BN_clear_free(&outpt->hash);
}

void bp_txin_init(struct bp_txin *txin)
{
	memset(txin, 0, sizeof(*txin));
	bp_outpt_init(&txin->prevout);
}

bool deser_bp_txin(struct bp_txin *txin, struct buffer *buf)
{
	if (!deser_bp_outpt(&txin->prevout, buf)) return false;
	if (!deser_varstr(&txin->scriptSig, buf)) return false;
	if (!deser_u32(&txin->nSequence, buf)) return false;
	return true;
}

void ser_bp_txin(GString *s, const struct bp_txin *txin)
{
	ser_bp_outpt(s, &txin->prevout);
	ser_varstr(s, txin->scriptSig);
	ser_u32(s, txin->nSequence);
}

void bp_txin_free(struct bp_txin *txin)
{
	bp_outpt_free(&txin->prevout);

	if (txin->scriptSig) {
		g_string_free(txin->scriptSig, TRUE);
		txin->scriptSig = NULL;
	}
}

void bp_txout_init(struct bp_txout *txout)
{
	memset(txout, 0, sizeof(*txout));
}

bool deser_bp_txout(struct bp_txout *txout, struct buffer *buf)
{
	if (!deser_s64(&txout->nValue, buf)) return false;
	if (!deser_varstr(&txout->scriptPubKey, buf)) return false;
	return true;
}

void ser_bp_txout(GString *s, const struct bp_txout *txout)
{
	ser_s64(s, txout->nValue);
	ser_varstr(s, txout->scriptPubKey);
}

void bp_txout_free(struct bp_txout *txout)
{
	if (txout->scriptPubKey) {
		g_string_free(txout->scriptPubKey, TRUE);
		txout->scriptPubKey = NULL;
	}
}

void bp_tx_init(struct bp_tx *tx)
{
	memset(tx, 0, sizeof(*tx));
}

bool deser_bp_tx(struct bp_tx *tx, struct buffer *buf)
{
	tx->vin = g_ptr_array_new_full(8, g_free);
	tx->vout = g_ptr_array_new_full(8, g_free);

	if (!deser_u32(&tx->nVersion, buf)) return false;

	uint32_t vlen;
	if (!deser_varlen(&vlen, buf)) return false;

	unsigned int i;
	for (i = 0; i < vlen; i++) {
		struct bp_txin *txin;

		txin = calloc(1, sizeof(*txin));
		bp_txin_init(txin);
		if (!deser_bp_txin(txin, buf)) {
			free(txin);
			goto err_out;
		}

		g_ptr_array_add(tx->vin, txin);
	}

	if (!deser_varlen(&vlen, buf)) return false;

	for (i = 0; i < vlen; i++) {
		struct bp_txout *txout;

		txout = calloc(1, sizeof(*txout));
		bp_txout_init(txout);
		if (!deser_bp_txout(txout, buf)) {
			free(txout);
			goto err_out;
		}

		g_ptr_array_add(tx->vout, txout);
	}

	if (!deser_u32(&tx->nLockTime, buf)) return false;
	return true;

err_out:
	bp_tx_free(tx);
	return false;
}

void ser_bp_tx(GString *s, const struct bp_tx *tx)
{
	ser_u32(s, tx->nVersion);

	unsigned int i;
	if (tx->vin) {
		for (i = 0; i < tx->vin->len; i++) {
			struct bp_txin *txin;

			txin = g_ptr_array_index(tx->vin, i);
			ser_bp_txin(s, txin);
		}
	}

	if (tx->vout) {
		for (i = 0; i < tx->vout->len; i++) {
			struct bp_txout *txout;

			txout = g_ptr_array_index(tx->vout, i);
			ser_bp_txout(s, txout);
		}
	}

	ser_u32(s, tx->nLockTime);
}

void bp_tx_free(struct bp_tx *tx)
{
	unsigned int i;

	if (tx->vin) {
		for (i = 0; i < tx->vin->len; i++) {
			struct bp_txin *txin;

			txin = g_ptr_array_index(tx->vin, i);
			bp_txin_free(txin);
		}

		g_ptr_array_free(tx->vin, TRUE);

		tx->vin = NULL;
	}

	if (tx->vout) {
		for (i = 0; i < tx->vout->len; i++) {
			struct bp_txout *txout;

			txout = g_ptr_array_index(tx->vout, i);
			bp_txout_free(txout);
		}

		g_ptr_array_free(tx->vout, TRUE);

		tx->vout = NULL;
	}
}

void bp_block_init(struct bp_block *block)
{
	memset(block, 0, sizeof(*block));
	BN_init(&block->hashPrevBlock);
	BN_init(&block->hashMerkleRoot);
}

bool deser_bp_block(struct bp_block *block, struct buffer *buf)
{
	block->vtx = g_ptr_array_new_full(512, g_free);

	if (!deser_u32(&block->nVersion, buf)) return false;
	if (!deser_u256(&block->hashPrevBlock, buf)) return false;
	if (!deser_u256(&block->hashMerkleRoot, buf)) return false;
	if (!deser_u32(&block->nTime, buf)) return false;
	if (!deser_u32(&block->nBits, buf)) return false;
	if (!deser_u32(&block->nNonce, buf)) return false;

	uint32_t vlen;
	if (!deser_varlen(&vlen, buf)) return false;

	unsigned int i;
	for (i = 0; i < vlen; i++) {
		struct bp_tx *tx;

		tx = calloc(1, sizeof(*tx));
		bp_tx_init(tx);
		if (!deser_bp_tx(tx, buf)) {
			free(tx);
			goto err_out;
		}

		g_ptr_array_add(block->vtx, tx);
	}

	return true;

err_out:
	bp_block_free(block);
	return false;
}

void ser_bp_block(GString *s, const struct bp_block *block)
{
	ser_u32(s, block->nVersion);
	ser_u256(s, &block->hashPrevBlock);
	ser_u256(s, &block->hashMerkleRoot);
	ser_u32(s, block->nTime);
	ser_u32(s, block->nBits);
	ser_u32(s, block->nNonce);

	unsigned int i;
	if (block->vtx) {
		for (i = 0; i < block->vtx->len; i++) {
			struct bp_tx *tx;

			tx = g_ptr_array_index(block->vtx, i);
			ser_bp_tx(s, tx);
		}
	}
}

void bp_block_free(struct bp_block *block)
{
	unsigned int i;

	if (block->vtx) {
		for (i = 0; i < block->vtx->len; i++) {
			struct bp_tx *tx;

			tx = g_ptr_array_index(block->vtx, i);
			bp_tx_free(tx);
		}

		g_ptr_array_free(block->vtx, TRUE);

		block->vtx = NULL;
	}
}

