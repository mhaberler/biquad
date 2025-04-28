#include "BiQuad.h"

BiQuad::BiQuad() {
    resetStateOnGainChange = true;
    set( 1.0, 0.0, 0.0, 0.0, 0.0 );
}

BiQuad::BiQuad(double b0, double b1, double b2, double a1, double a2) {
    resetStateOnGainChange = true;
    set( b0, b1, b2, a1, a2 );
}

BiQuad::BiQuad(double b0, double b1, double b2, double a0, double a1, double a2) {
    resetStateOnGainChange = true;
    set( b0/a0, b1/a0, b2/a0, a1/a0, a2/a0 );
}

void BiQuad::PIDF( double Kp, double Ki, double Kd, double N, double Ts  ) {

    double b0, b1, b2, bd, a1, a2;

    a1 = -4.0/(N*Ts+2.0);
    a2 = -(N*Ts-2.0)/(N*Ts+2.0);

    bd = ( N*Ts+2.0 );

    b0 = ( 4.0*Kp + 4.0*Kd*N + 2.0*Ki*Ts + 2.0*Kp*N*Ts + Ki*N*Ts*Ts )/(2.0*bd);
    b1 = ( Ki*N*Ts*Ts - 4.0*Kp - 4.0*Kd*N )/bd;
    b2 = ( 4.0*Kp + 4.0*Kd*N - 2*Ki*Ts - 2*Kp*N*Ts + Ki*N*Ts*Ts )/(2.0*bd);

    set( b0, b1, b2, a1, a2 );

};

void BiQuad::set(double b0, double b1, double b2, double a1, double a2) {

    B[0] = b0;
    B[1] = b1;
    B[2] = b2;
    A[0] = a1;
    A[1] = a2;

    if( resetStateOnGainChange ) {
        wz[0] = 0;
        wz[1] = 0;
    }

}

double BiQuad::step(double x) {

    double y;

    /* Direct form II transposed */
    y = B[0] * x + wz[0];
    wz[0] = B[1] * x - A[0] * y + wz[1];
    wz[1] = B[2] * x - A[1] * y;

    return y;

}

std::vector< std::complex<double> > BiQuad::poles() {

    std::vector< std::complex<double> > poles;

    std::complex<double> b2(A[0]*A[0],0);
    std::complex<double> ds = std::sqrt( b2-4*A[1] );

    poles.push_back( 0.5*(-A[0]+ds) );
    poles.push_back( 0.5*(-A[0]-ds) );

    return poles;

}

std::vector< std::complex<double> > BiQuad::zeros() {

    std::vector< std::complex<double> > zeros;

    std::complex<double> b2(B[1]*B[1],0);
    std::complex<double> ds = std::sqrt( b2-4*B[0]*B[2] );

    zeros.push_back( 0.5*(-B[1]+ds)/B[0] );
    zeros.push_back( 0.5*(-B[1]-ds)/B[0] );

    return zeros;

}

bool BiQuad::stable() {
    bool stable = true;
    std::vector< std::complex<double> > ps = poles();
    for( size_t i = 0; i < ps.size(); i++ )
        stable = stable & ( std::abs( ps[i] ) < 1 );
    return stable;
}

void BiQuad::setResetStateOnGainChange( bool v ) {
    resetStateOnGainChange = v;
}

BiQuadChain &BiQuadChain::add(BiQuad *bq) {
    biquads.push_back( bq );
    return *this;
}

BiQuadChain operator*( BiQuad &bq1, BiQuad &bq2 ) {
    BiQuadChain bqc;
    bqc.add( &bq1 ).add( &bq2 );
    return bqc;
}

double BiQuadChain::step(double x) {

    size_t i;
    size_t bqs;

    bqs = biquads.size();

    for( i = 0; i < bqs; i++ )
        x = biquads[i]->step( x );

    return x;
}

std::vector< std::complex<double> > BiQuadChain::poles_zeros( bool zeros ) {

    std::vector< std::complex<double> > chain, bq;
    size_t i;
    size_t bqs;

    bqs = biquads.size();

    for( i = 0; i < bqs; i++ ) {
        bq = ( zeros ) ? biquads[ i ]->zeros() : biquads[ i ]->poles();
        chain.insert( chain.end(), bq.begin(), bq.end() );
    }

    return chain;

}

std::vector< std::complex<double> > BiQuadChain::poles() {
    return poles_zeros( false );
}

std::vector< std::complex<double> > BiQuadChain::zeros() {
    return poles_zeros( true );
}

bool BiQuadChain::stable() {
    bool stable = true;
    for( size_t i = 0; i < biquads.size(); i++ )
        stable = stable & biquads[i]->stable();
    return stable;
}

BiQuadChain& BiQuadChain::operator*( BiQuad& bq ) {
    add( &bq );
    return *this;
}



// Move setCoefficients implementation here, outside the class
bool setCoefficients(bq_type_t type,
                     double Fc,
                     double Q,
                     double peakGainDB,
                     BiQuad& bq) {
    double norm;
    double V = pow(10, fabs(peakGainDB) / 20.0);
    double K = tan(M_PI * Fc);
    double a0, a1, a2, b1, b2;
    switch (type) {
        case bq_type_lowpass:
            norm = 1 / (1 + K / Q + K * K);
            a0 = K * K * norm;
            a1 = 2 * a0;
            a2 = a0;
            b1 = 2 * (K * K - 1) * norm;
            b2 = (1 - K / Q + K * K) * norm;
            break;

        case bq_type_highpass:
            norm = 1 / (1 + K / Q + K * K);
            a0 = 1 * norm;
            a1 = -2 * a0;
            a2 = a0;
            b1 = 2 * (K * K - 1) * norm;
            b2 = (1 - K / Q + K * K) * norm;
            break;

        case bq_type_bandpass:
            norm = 1 / (1 + K / Q + K * K);
            a0 = K / Q * norm;
            a1 = 0;
            a2 = -a0;
            b1 = 2 * (K * K - 1) * norm;
            b2 = (1 - K / Q + K * K) * norm;
            break;

        case bq_type_notch:
            norm = 1 / (1 + K / Q + K * K);
            a0 = (1 + K * K) * norm;
            a1 = 2 * (K * K - 1) * norm;
            a2 = a0;
            b1 = a1;
            b2 = (1 - K / Q + K * K) * norm;
            break;

        case bq_type_peak:
            if (peakGainDB >= 0) {    // boost
                norm = 1 / (1 + 1/Q * K + K * K);
                a0 = (1 + V/Q * K + K * K) * norm;
                a1 = 2 * (K * K - 1) * norm;
                a2 = (1 - V/Q * K + K * K) * norm;
                b1 = a1;
                b2 = (1 - 1/Q * K + K * K) * norm;
            } else {  // cut
                norm = 1 / (1 + V/Q * K + K * K);
                a0 = (1 + 1/Q * K + K * K) * norm;
                a1 = 2 * (K * K - 1) * norm;
                a2 = (1 - 1/Q * K + K * K) * norm;
                b1 = a1;
                b2 = (1 - V/Q * K + K * K) * norm;
            }
            break;
        case bq_type_lowshelf:
            if (peakGainDB >= 0) {    // boost
                norm = 1 / (1 + sqrt(2) * K + K * K);
                a0 = (1 + sqrt(2*V) * K + V * K * K) * norm;
                a1 = 2 * (V * K * K - 1) * norm;
                a2 = (1 - sqrt(2*V) * K + V * K * K) * norm;
                b1 = 2 * (K * K - 1) * norm;
                b2 = (1 - sqrt(2) * K + K * K) * norm;
            } else {  // cut
                norm = 1 / (1 + sqrt(2*V) * K + V * K * K);
                a0 = (1 + sqrt(2) * K + K * K) * norm;
                a1 = 2 * (K * K - 1) * norm;
                a2 = (1 - sqrt(2) * K + K * K) * norm;
                b1 = 2 * (V * K * K - 1) * norm;
                b2 = (1 - sqrt(2*V) * K + V * K * K) * norm;
            }
            break;
        case bq_type_highshelf:
            if (peakGainDB >= 0) {    // boost
                norm = 1 / (1 + sqrt(2) * K + K * K);
                a0 = (V + sqrt(2*V) * K + K * K) * norm;
                a1 = 2 * (K * K - V) * norm;
                a2 = (V - sqrt(2*V) * K + K * K) * norm;
                b1 = 2 * (K * K - 1) * norm;
                b2 = (1 - sqrt(2) * K + K * K) * norm;
            } else {  // cut
                norm = 1 / (V + sqrt(2*V) * K + K * K);
                a0 = (1 + sqrt(2) * K + K * K) * norm;
                a1 = 2 * (K * K - 1) * norm;
                a2 = (1 - sqrt(2) * K + K * K) * norm;
                b1 = 2 * (K * K - V) * norm;
                b2 = (V - sqrt(2*V) * K + K * K) * norm;
            }
            break;
        case bq_type_unity:
            a0 = 1.0;
            a1 = 0.0;
            a2 = 0.0;
            b1 = 0.0;
            b2 = 0.0;
            break;
        default:
            return false;
    }
    bq.set( a0, a1, a2, b1, b2 );
    return true;
}