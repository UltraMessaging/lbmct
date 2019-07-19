package com.latencybusters.lbmct;
/*
 * See https://github.com/UltraMessaging/lbmct for code and documentation.
 *
 * Copyright (c) 2018-2019 Informatica Corporation. All Rights Reserved.
 * Permission is granted to licensees to use or alter this software for
 * any purpose, including commercial applications, according to the terms
 * laid out in the Software License Agreement.
 -
 - This receiver code example is provided by Informatica for educational
 - and evaluation purposes only.
 -
 - THE SOFTWARE IS PROVIDED "AS IS" AND INFORMATICA DISCLAIMS ALL WARRANTIES
 - EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF
 - NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 - INFORMATICA DOES NOT WARRANT THAT USE OF THE SOFTWARE WILL BE UNINTERRUPTED
 - OR ERROR-FREE.  INFORMATICA SHALL NOT, UNDER ANY CIRCUMSTANCES, BE
 - LIABLE TO LICENSEE FOR LOST PROFITS, CONSEQUENTIAL, INCIDENTAL, SPECIAL OR
 - INDIRECT DAMAGES ARISING OUT OF OR RELATED TO THIS AGREEMENT OR THE
 - TRANSACTIONS CONTEMPLATED HEREUNDER, EVEN IF INFORMATICA HAS BEEN APPRISED
 - OF THE LIKELIHOOD OF SUCH DAMAGES.
 */

/**
 * Container object holding configuration of an instance of {@code LbmCt}.
 * The caller creates an config instance and sets desired options, and then passes it to {@link LbmCt#start}.
 * <p>
 * Note that the CT object retains a reference to the passed-in configuration object, so it is not recommended to modify
 * options after the {@code LbmCt} is created, except for testing purposes.
 * Also, if creating multiple CT objects, it is generally recommended to create separate configuration objects,
 * one for each CT object (unless they all share exactly the same configuration).
 *
 */
@SuppressWarnings("WeakerAccess")  // public API.
public class LbmCtConfig {
  /**
   * Bit set in {@code testBits} to enable debug event logging.
   * This bit controls the operation of {@code dbg} and {@code debugQ}.
   * See <a href="https://ultramessaging.github.io/lbmct/doc/Java_Userguide.html#debugging">Debugging</a>
   * for information on using this debugging feature.
   * See {@link #setTestBits}.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public final static int TEST_BITS_DEBUG   = 0x00000001;
  /**
   * Bit set in {@code testBits} to disable sending of CREQ handshake.
   * This is to test retry logic.
   * See {@link #setTestBits}.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public final static int TEST_BITS_NO_CREQ = 0x00000002;
  /**
   * Bit set in {@code testBits} to disable sending of CRSP handshake.
   * This is to test retry logic.
   * See {@link #setTestBits}.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public final static int TEST_BITS_NO_CRSP = 0x00000004;
  /**
   * Bit set in {@code testBits} to disable sending of COK handshake.
   * This is to test retry logic.
   * See {@link #setTestBits}.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public final static int TEST_BITS_NO_COK  = 0x00000008;
  /**
   * Bit set in {@code testBits} to disable sending of DREQ handshake.
   * This is to test retry logic.
   * See {@link #setTestBits}.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public final static int TEST_BITS_NO_DREQ = 0x00000010;
  /**
   * Bit set in {@code testBits} to disable sending of DRSP handshake.
   * This is to test retry logic.
   * See {@link #setTestBits}.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public final static int TEST_BITS_NO_DRSP = 0x00000020;
  /**
   * Bit set in {@code testBits} to disable sending of DOK handshake.
   * This is to test retry logic.
   * See {@link #setTestBits}.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public final static int TEST_BITS_NO_DOK  = 0x00000040;

  // Default values for config options.
  /**
   * Initial value for {@code testBits}; all testing behavior disabled.
   * See {@link #setTestBits}.
   */
  final static int CT_CONFIG_DEFAULT_TEST_BITS  = 0x00000000;
  /**
   * Initial value for {@code domainId}; no UM domain ID used.
   * See {@link #setDomainId}.
   */
  final static int CT_CONFIG_DEFAULT_DOMAIN_ID  = -1;
  /**
   * Initial value for {@code delayCreq}; receiver waits this long before
   * sending CREQ handshake.
   * The default value should be long enough to allow the transport session to be established, and even in those
   * cases where more time is needed, the retry logic will provide that time.
   * See {@link #setDelayCreq}.
   */
  final static int CT_CONFIG_DEFAULT_DELAY_CREQ = 10;    // 10 ms
  /**
   * Initial value for {@code retryIvl}; wait this long for a handshake
   * operation before re-trying.
   * Retries should happen only very rarely.
   * See {@link #setRetryIvl}.
   */
  final static int CT_CONFIG_DEFAULT_RETRY_IVL  = 1000;  // 1 sec
  /**
   * Initial value for {@code maxTries}; try a handshake operation this
   * many times before giving up.
   * Be aware that if a network outage lasts longer than {@code maxTries} * {@code retryIvl}, but the transport
   * session does not time out, the connection could fail to be established.
   * The defaults were chosen to be longer than the default transport session timeouts for LBT-RM and LBT-RU.
   * See {@link #setMaxTries}.
   */
  final static int CT_CONFIG_DEFAULT_MAX_TRIES  = 35;
  /**
   * Initial value for {@code preDelivery}; disable delivery of UM messages
   * to receiver when not connected.
   * See {@link #setPreDelivery}.
   */
  final static int CT_CONFIG_DEFAULT_PRE_DELIVERY = 0;

  // Bit map of options used for testing or debugging
  private int testBits = CT_CONFIG_DEFAULT_TEST_BITS;
  private int domainId = CT_CONFIG_DEFAULT_DOMAIN_ID;
  private int delayCreq = CT_CONFIG_DEFAULT_DELAY_CREQ;
  private int retryIvl = CT_CONFIG_DEFAULT_RETRY_IVL;
  private int maxTries = CT_CONFIG_DEFAULT_MAX_TRIES;
  private int preDelivery = CT_CONFIG_DEFAULT_PRE_DELIVERY;

  /**
   * Constructor for LbmCt configuration object, initialized to default values.
   * Use setters to override defaults.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public LbmCtConfig() {
  }

  /**
   * Override the default value for the configuration object's test bits.
   * See {@link #CT_CONFIG_DEFAULT_TEST_BITS}.
   * <p>
   * @param testBits  Integer bit map of debug behaviors to enable.
   *     One or more of the following ORed together:
   *     {@link #TEST_BITS_DEBUG}, {@link #TEST_BITS_NO_CREQ}, {@link #TEST_BITS_NO_CRSP}, {@link #TEST_BITS_NO_COK},
   *     {@link #TEST_BITS_NO_DREQ}, {@link #TEST_BITS_NO_DRSP}, {@link #TEST_BITS_NO_DOK}.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public void setTestBits(int testBits) { this.testBits = testBits; }

  /**
   * Override the default value for the configuration object's Topic Resolution Domain ID.
   * If not set (defaults to -1), the CT layer does not assume a domain ID, and will rely on the
   * <a href="https://ultramessaging.github.io/currdoc/doc/Design/umobjects.html#sendingtosources">sending to sources</a>
   * feature.
   * See <a href="https://ultramessaging.github.io/lbmct/doc/Domain_ID.html">Domain ID</a> for more information.
   * <p>
   * @param domainId  Topic Resolution Domain ID.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public void setDomainId(int domainId) { this.domainId = domainId; }

  /**
   * Override the default value for the configuration object's delay before a receiver sends its the initial connection
   * request handshake.
   * <p>
   * @param delayCreq  Time in milliseconds to delay.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public void setDelayCreq(int delayCreq) { this.delayCreq = delayCreq; }

  /**
   * Override the default value for the configuration object's time between handshake attempts.
   * When a connected source and receiver are connecting or disconnecting, they exchange handshake messages to
   * synchronize state.
   * If a CT source or receiver sends a handshake and expects a response but does not get one within
   * {@code retryIvl} milliseconds, the operation will be retried.
   * See also {@link #setMaxTries}.
   * <p>
   * @param retryIvl  Time in milliseconds to wait.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public void setRetryIvl(int retryIvl) { this.retryIvl = retryIvl; }

  /**
   * Override the default value for the configuration object's number of tries.
   * When a connected source and receiver are connecting or disconnecting, they exchange handshake messages to
   * synchronize state.
   * If a CT source or receiver sends a handshake and expects a response but does not get one within
   * {@code retryIvl} milliseconds, the operation will be retried.
   * After {@code maxTries} attempts, CT will give up, log an error, and stop retrying.
   * <p>
   * @param maxTries  Number of times to attempt handshakes.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public void setMaxTries(int maxTries) { this.maxTries = maxTries; }

  /**
   * Override the default value for the configuration object's received message pre-connected delivery behavior.
   * Normally, a Connected Topics Receiver will not deliver messages received prior to a connection being established.
   * Setting {@code preDelivery} to 1 enables the CT receiver to deliver messages received outside of the connected
   * state.
   * This can enable a CT receiver to get messages from a non-CT source,
   * although this use case is not recommended. See <a href="https://ultramessaging.github.io/lbmct/#interoperability"></a>.
   *
   * @param preDelivery  1=deliver messages outside of connection; 0=do not deliver.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public void setPreDelivery(int preDelivery) { this.preDelivery = preDelivery; }

  /**
   * Gets the current value for the configuration object's test bits.
   * This is important for cases where the user wants to set or clear an individual bit, leaving the rest of the bits
   * undisturbed.
   * @return  Current value for test bits.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public int getTestBits() { return testBits; }

  /**
   * Gets domain ID.
   * @return  Current domain ID.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public int getDomainId() { return domainId; }

  /**
   * Gets delay, in milliseconds, before a receiver sends its the initial connection request handshake.
   * @return Time in milliseconds to delay.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public int getDelayCreq() { return delayCreq; }

  /**
   * Gets time, in milliseconds, between handshake attempts.
   * @return  Time in milliseconds between attempts.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public int getRetryIvl() { return retryIvl; }

  /**
   * Gets maximum number of handshake attempts.
   * @return  Maximum number of attempts.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public int getMaxTries() { return maxTries; }

  /**
   * Gets message delivery with non-connected sources.
   * @return  1=deliver messages outside of connection; 0=do not deliver.
   */
  @SuppressWarnings("WeakerAccess")  // public API.
  public int getPreDelivery() { return preDelivery; }
}
